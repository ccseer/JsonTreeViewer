#include "jsontreeviewer.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>

#include "jsontreemodel.h"
#include "jsontreeview.h"
#include "loadworker.h"
#include "seer/viewerhelper.h"

#define qprintt qDebug() << "[JsonTreeViewer]"

JsonTreeViewer::JsonTreeViewer(QWidget* parent)
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
    QHBoxLayout* layout_bg = new QHBoxLayout;
    m_top.wnd_bg->setLayout(layout_bg);
    m_top.filter = new QLineEdit(this);
    layout_bg->addWidget(m_top.filter);

    m_top.filter->setPlaceholderText("Filter...");
    m_top.filter->setClearButtonEnabled(true);
}

QSize JsonTreeViewer::getContentSize() const
{
    const auto sz_def = options()->dpr() * QSize{600, 800};
    auto cmd
        = options()->property(ViewOptionsKeys::kKeyPluginCmd).toStringList();
    if (!cmd.isEmpty()) {
        auto parsed = seer::parseViewerSizeFromConfig(cmd);
        qprintt << "getContentSize: parsed" << parsed << cmd;
        if (parsed.isValid()) {
            return parsed;
        }
    }
    return sz_def;
}

void JsonTreeViewer::updateDPR(qreal r)
{
    layout()->setSpacing(6 * r);
    auto font = qApp->font();
    font.setPixelSize(12 * r);
    if (m_top.wnd_bg) {
        auto height_top = 30 * r;
        m_top.wnd_bg->setFixedHeight(height_top);
        auto m = 9 * r;
        m_top.wnd_bg->layout()->setContentsMargins(m, 0, m, 0);
        m_top.wnd_bg->layout()->setSpacing(0);
        m_top.filter->setFont(font);
        m_top.filter->setFixedHeight(height_top);
    }
    m_view->upadteDPR(r);

    if (m_status_bar) {
        auto sbFont = qApp->font();
        sbFont.setPixelSize(11 * r);
        m_status_bar->setFont(sbFont);
        if (auto parent = qobject_cast<QWidget*>(m_status_bar->parent())) {
            parent->setFixedHeight(20 * r);
            parent->layout()->setContentsMargins(6 * r, 0, 6 * r, 0);
        }
    }

    if (m_warning_icon) {
        auto warnFont = qApp->font();
        warnFont.setPixelSize(13 * r);
        warnFont.setBold(true);
        m_warning_icon->setFont(warnFont);
    }

    if (m_btn_text_view) {
        m_btn_text_view->setFixedSize(
            m_btn_text_view->fontMetrics().horizontalAdvance(
                m_btn_text_view->text())
                + 8 * r * 2,
            30 * r);
    }
}

void JsonTreeViewer::loadImpl(QBoxLayout* lay_content, QHBoxLayout* lay_ctrlbar)
{
    initTopWnd();
    lay_content->setSpacing(0);
    lay_content->addWidget(m_top.wnd_bg);

    JsonTreeModel* m = new JsonTreeModel(this);

    auto proxy_model = new TreeFilterProxyModel(this);
    proxy_model->setSourceModel(m);
    QTimer* timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(m_top.filter, &QLineEdit::textChanged, this,
            [timer](const QString&) { timer->start(300); });
    connect(timer, &QTimer::timeout, proxy_model, [proxy_model, this] {
        proxy_model->updateFilter(m_top.filter->text());
    });

    m_view = new JsonTreeView(this);
    m_view->setCopyActions(m->supportedActions());
    m_view->setModel(proxy_model);
    lay_content->addWidget(m_view);

    // Status bar
    QWidget* statusBarWidget  = new QWidget(this);
    QHBoxLayout* statusLayout = new QHBoxLayout(statusBarWidget);
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->setSpacing(0);
    m_status_bar = new QLabel(this);
    m_status_bar->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_status_bar->setTextInteractionFlags(Qt::TextSelectableByMouse);
    statusLayout->addWidget(m_status_bar, 1);
    lay_content->addWidget(statusBarWidget);

    connect(
        m_view->selectionModel(), &QItemSelectionModel::currentChanged, this,
        [this, m, proxy_model](const QModelIndex& current, const QModelIndex&) {
            if (!current.isValid()) {
                m_status_bar->clear();
                return;
            }
            QModelIndex src = proxy_model->mapToSource(current);
            QString path    = m->getPath(src);
            QString key     = m->getKey(src);
            QString val     = m->getValue(src);

            QString text = path.isEmpty() ? key : path;
            if (!val.isEmpty()) {
                constexpr int MAX_VAL = 80;
                if (val.length() > MAX_VAL)
                    val = val.left(MAX_VAL) + "...";
                text += "  =  " + val;
            }
            m_status_bar->setText(text);
        });

    // Connect copy signals
    connect(m_view, &JsonTreeView::copyKeyRequested, this,
            [this, m](const QModelIndex& proxyIndex) {
                auto* proxy
                    = qobject_cast<TreeFilterProxyModel*>(m_view->model());
                if (!proxy)
                    return;
                QString key = m->getKey(proxy->mapToSource(proxyIndex));
                if (!key.isEmpty())
                    QApplication::clipboard()->setText(key);
            });

    connect(m_view, &JsonTreeView::copyValueRequested, this,
            [this, m](const QModelIndex& proxyIndex) {
                auto* proxy
                    = qobject_cast<TreeFilterProxyModel*>(m_view->model());
                if (!proxy)
                    return;
                QString value = m->getValue(proxy->mapToSource(proxyIndex));
                if (!value.isEmpty())
                    QApplication::clipboard()->setText(value);
            });

    connect(m_view, &JsonTreeView::copyPathRequested, this,
            [this, m](const QModelIndex& proxyIndex) {
                auto* proxy
                    = qobject_cast<TreeFilterProxyModel*>(m_view->model());
                if (!proxy)
                    return;
                QString path = m->getPath(proxy->mapToSource(proxyIndex));
                if (!path.isEmpty())
                    QApplication::clipboard()->setText(path);
            });

    connect(
        m_view, &JsonTreeView::copySubtreeRequested, this,
        [this, m](const QModelIndex& proxyIndex) {
            auto* proxy = qobject_cast<TreeFilterProxyModel*>(m_view->model());
            if (!proxy)
                return;
            bool success = false;
            QString errorMsg;
            QString subtree = m->getSubtree(proxy->mapToSource(proxyIndex),
                                            &success, &errorMsg);
            if (success) {
                QApplication::clipboard()->setText(subtree);
            }
            else {
                emit sigCommand(ViewCommandType::VCT_ShowToastMsg,
                                tr("Copy Subtree Failed") + "\n" + errorMsg);
            }
        });

    connect(
        m_view, &JsonTreeView::copyKeyValueRequested, this,
        [this, m](const QModelIndex& proxyIndex) {
            auto* proxy = qobject_cast<TreeFilterProxyModel*>(m_view->model());
            if (!proxy)
                return;
            bool success = false;
            QString errorMsg;
            QString kv = m->getKeyValue(proxy->mapToSource(proxyIndex),
                                        &success, &errorMsg);
            if (success) {
                QApplication::clipboard()->setText(kv);
            }
            else {
                emit sigCommand(ViewCommandType::VCT_ShowToastMsg,
                                tr("Copy Key:Value Failed") + "\n" + errorMsg);
            }
        });

    if (lay_ctrlbar) {
        lay_ctrlbar->addStretch();
        m_btn_text_view = new QPushButton(this);
        m_btn_text_view->setText("Text View");
        lay_ctrlbar->addWidget(m_btn_text_view);
        connect(m_btn_text_view, &QPushButton::clicked, this,
                &JsonTreeViewer::onTextViewBtnClicked);
    }

    updateDPR(options()->dpr());
    updateTheme(options()->theme());

    // Start background loading
    startBackgroundLoad(m, options()->path());
    // VCV_Loaded will be emitted by startBackgroundLoad after loading completes
}

void JsonTreeViewer::onTextViewBtnClicked()
{
    // built-in viewer
    emit sigCommand(ViewCommandType::VCT_LoadViewerWithNewType,
                    QString("Text"));
}

void JsonTreeViewer::startBackgroundLoad(JsonTreeModel* model,
                                         const QString& path)
{
    qprintt << "=== [BG LOAD START] ===" << path;

    if (m_status_bar) {
        m_status_bar->setText(tr("Loading..."));
    }

    connect(
        model, &JsonTreeModel::loadFinished, this,
        [this, model](bool success, qint64) {
            qprintt << "[BG LOAD] Back in main thread, updating UI...";
            QElapsedTimer uiTimer;
            uiTimer.start();

            // Back in main thread for UI updates
            if (!success) {
                if (m_status_bar) {
                    m_status_bar->setText(tr("Failed to load JSON file"));
                }
                emit sigCommand(ViewCommandType::VCT_StateChange, VCV_Error);
                qprintt << "=== [BG LOAD END] Failed ===";
                return;
            }

            m_view->setCopyActions(model->supportedActions());

            qprintt << "[BG LOAD] Adding warning icon if needed...";

            // Add warning icon for large/extreme files
            using FM = JsonTreeModel::FileMode;
            if ((model->fileMode() == FM::Large
                 || model->fileMode() == FM::Extreme)
                && (!m_warning_icon && m_status_bar
                    && m_status_bar->parent())) {
                m_warning_icon = new QLabel(this);
                m_warning_icon->setText(" ⚠ ");
                m_warning_icon->setAlignment(Qt::AlignCenter);

                QStringList warnings;
                if (model->fileMode() == FM::Extreme) {
                    warnings << tr("Extreme file (>1 GB):")
                             << tr("• Only Key/Value copy supported")
                             << tr("• Path and Subtree operations disabled");
                }
                else {
                    warnings << tr("Large file (>100 MB):")
                             << tr("• Path copy supported")
                             << tr("• Subtree and Key:Value copy disabled");
                }
                m_warning_icon->setToolTip(warnings.join("\n"));

                // Add to status bar layout
                if (auto* wgt = qobject_cast<QWidget*>(m_status_bar->parent());
                    wgt && wgt->layout()) {
                    wgt->layout()->addWidget(m_warning_icon);
                }
            }

            qprintt << "[BG LOAD] UI update completed in" << uiTimer.elapsed()
                    << "ms";

            if (m_status_bar) {
                m_status_bar->setText(tr("Ready"));
            }

            qprintt << "=== [BG LOAD END] Success ===";
            emit sigCommand(ViewCommandType::VCT_StateChange, VCV_Loaded);
        },
        Qt::SingleShotConnection);

    // Start loading (model handles background thread internally)
    qprintt << "[BG LOAD] Starting model load...";
    model->load(path);
}
