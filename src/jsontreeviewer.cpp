#include "jsontreeviewer.h"

#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimer>

#include "jsontreemodel.h"
#include "jsontreeview.h"

#define qprintt qDebug() << "[JsonTreeViewer]"
// TODO: threading?

JsonTreeViewer::JsonTreeViewer(QWidget *parent)
    : ViewerBase(parent), m_btn_text_view(nullptr), m_view(nullptr)
{
    qprintt << this;
}

JsonTreeViewer::~JsonTreeViewer()
{
    qprintt << "~" << this;
}

void JsonTreeViewer::initTopWnd()
{
    m_top.wnd_bg           = new QWidget(this);
    QHBoxLayout *layout_bg = new QHBoxLayout;
    m_top.wnd_bg->setLayout(layout_bg);
    m_top.filter = new QLineEdit(this);
    layout_bg->addWidget(m_top.filter);
    m_top.btn = new QPushButton(this);
    layout_bg->addWidget(m_top.btn);

    m_top.filter->setPlaceholderText("Filter...");
    m_top.filter->setClearButtonEnabled(true);

    m_top.btn->setText("Load Entire File");
    connect(m_top.btn, &QPushButton::clicked, this, [this]() {
        if (auto tfpm = qobject_cast<TreeFilterProxyModel *>(m_view->model())) {
            if (auto jtm = qobject_cast<JsonTreeModel *>(tfpm->sourceModel())) {
                m_top.btn->setText("Loading...");
                m_top.btn->setVisible(false);
                m_view->blockSignals(true);
                jtm->loadEverything();
                m_view->blockSignals(false);
            }
        }
    });
    // too slow, take 30s to load 10MB file
    m_top.btn->setVisible(false);
}

QSize JsonTreeViewer::getContentSize() const
{
    return QSize{600, 800} * m_d->d->dpr;
}

void JsonTreeViewer::updateDPR(qreal r)
{
    m_d->d->dpr = r;
    
    layout()->setSpacing(6 * r);
    auto font = qApp->font();
    font.setPixelSize(12 * r);
    if (m_top.wnd_bg) {
        auto height_top = 30 * r;
        m_top.wnd_bg->setFixedHeight(height_top);
        auto m = 9 * r;
        m_top.wnd_bg->layout()->setContentsMargins(m, 0, m, 0);
        m_top.wnd_bg->layout()->setSpacing(0);
        m_top.btn->setFixedHeight(height_top);
        m_top.btn->setFont(font);
        m_top.filter->setFont(font);
        m_top.filter->setFixedHeight(height_top);
    }
    m_view->upadteDPR(r);

    if (m_btn_text_view) {
        m_btn_text_view->setFixedSize(
            m_btn_text_view->fontMetrics().horizontalAdvance(
                m_btn_text_view->text())
                + 8 * r * 2,
            30 * r);
    }
}

void JsonTreeViewer::loadImpl(QBoxLayout *lay_content, QHBoxLayout *lay_ctrlbar)
{
    initTopWnd();
    lay_content->addWidget(m_top.wnd_bg);
    JsonTreeModel *m = new JsonTreeModel(this);
    m->load(m_d->d->path);
    auto proxy_model = new TreeFilterProxyModel(this);
    proxy_model->setSourceModel(m);
    QTimer *timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(m_top.filter, &QLineEdit::textChanged, this,
            [timer](const QString &) { timer->start(300); });
    connect(timer, &QTimer::timeout, proxy_model, [proxy_model, this] {
        proxy_model->updateFilter(m_top.filter->text());
    });

    m_view = new JsonTreeView(this);
    m_view->setModel(proxy_model);
    lay_content->addWidget(m_view);

    if (lay_ctrlbar) {
        lay_ctrlbar->addStretch();
        m_btn_text_view = new QPushButton(this);
        m_btn_text_view->setText("Text View");
        lay_ctrlbar->addWidget(m_btn_text_view);
        connect(m_btn_text_view, &QPushButton::clicked, this,
                &JsonTreeViewer::onTextViewBtnClicked);
    }

    updateDPR(m_d->d->dpr);
    updateTheme(m_d->d->theme);

    emit sigCommand(ViewCommandType::VCT_StateChange, VCV_Loaded);
}

void JsonTreeViewer::onTextViewBtnClicked()
{
    // built-in viewer
    emit sigCommand(ViewCommandType::VCT_LoadViewerWithNewType,
                    QString("Text"));
}
