#include "searchpanel.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include "../jsontreemodel.h"
#include "../style_assets.h"
#include "searchresultdelegate.h"

#define qprintt qprint << "[SearchPanel]"

using namespace jtv::ui;

SearchPanel::SearchPanel(JsonTreeModel* model, QWidget* parent)
    : QWidget(parent), m_model_ref(model)
{
    setObjectName("searchPanel");
    auto* mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(0, 0, 0, 0);
    mainLay->setSpacing(0);

    m_results_model = new QStandardItemModel(this);

    // Banner UI
    m_banner = new QWidget(this);
    m_banner->setObjectName("searchBanner");
    mainLay->addWidget(m_banner);

    // Use GridLayout to stack progress fill behind controls
    auto* bannerGrid = new QGridLayout(m_banner);
    bannerGrid->setContentsMargins(0, 0, 0, 0);

    // Progress Fill (Bottom Layer)
    m_progress_fill = new QWidget(m_banner);
    m_progress_fill->setObjectName("searchProgressFill");
    m_progress_fill->setFixedWidth(0);
    bannerGrid->addWidget(m_progress_fill, 0, 0);

    // Controls (Top Layer)
    auto* controlsContainer = new QWidget(m_banner);
    auto* controlsLay       = new QHBoxLayout(controlsContainer);

    m_label_query = new QLabel(this);
    controlsLay->addWidget(m_label_query, 1);

    m_label_count = new QLabel(this);
    controlsLay->addWidget(m_label_count);

    m_btn_cancel = new QPushButton(this);
    m_btn_cancel->setToolTip(tr("Close Search"));
    controlsLay->addWidget(m_btn_cancel);

    bannerGrid->addWidget(controlsContainer, 0, 0);

    // List UI
    m_view = new QListView(this);
    m_view->setModel(m_results_model);
    m_view->setUniformItemSizes(true);
    m_view->setItemDelegate(new SearchResultDelegate(this));
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setLineWidth(0);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLay->addWidget(m_view);

    connect(m_view, &QListView::clicked, this, &SearchPanel::onResultClicked);
    connect(m_btn_cancel, &QPushButton::clicked, this, [this]() {
        qprintt << "Cancel button clicked";
        cancelSearch();
    });
}

SearchPanel::~SearchPanel()
{
    cancelSearch();
}

void SearchPanel::startSearch(std::shared_ptr<JsonViewerStrategy> strategy,
                              const SearchQuery& query)
{
    cancelSearch();

    m_results_model->clear();
    m_results_model->setHorizontalHeaderLabels({tr("Results")});

    m_label_query->setText(tr("Searching: %1").arg(query.text));
    m_label_count->setText(tr("Searching..."));
    m_progress_fill->setFixedWidth(0);
    m_progress_fill->show();
    m_btn_cancel->show();
    show();

    auto* thread                  = new JTVThread;
    QPointer<SearchWorker> worker = new SearchWorker(strategy, query);
    connect(this, &QObject::destroyed, [worker, thread]() {
        if (worker) {
            qprintt << "[SEARCH ASYNC] SearchPanel destroyed, requesting "
                       "worker to stop";
            thread->requestInterruption();
        }
    });
    connect(worker, &QObject::destroyed, thread, &QObject::deleteLater);
    connect(this, &SearchPanel::cancelRequested, this, [worker, thread]() {
        // Calling from main thread, otherwise the call will be queued
        if (worker) {
            qprintt << "[SEARCH ASYNC] Requesting worker to stop";
            thread->requestInterruption();
        }
    });
    // Business logic
    connect(worker, &SearchWorker::resultsFound, this,
            &SearchPanel::onResultsFound);
    connect(worker, &SearchWorker::progressUpdated, this, [this](int value) {
        int targetWidth = (m_banner->width() * value) / 100;
        m_progress_fill->setFixedWidth(targetWidth);
    });
    connect(worker, &SearchWorker::limitReached, this, [this](int max) {
        m_label_count->setText(tr("%1+ results").arg(max));
    });
    connect(worker, &SearchWorker::finished, this,
            &SearchPanel::onSearchFinished);
    connect(thread, &QThread::started, worker, &SearchWorker::process);

    worker->moveToThread(thread);
    thread->start();
}

void SearchPanel::cancelSearch()
{
    emit cancelRequested();
    m_progress_fill->setFixedWidth(0);
    hide();
}

void SearchPanel::onResultsFound(const QVector<SearchResult>& results)
{
    for (const auto& res : results) {
        QStandardItem* item = new QStandardItem();
        QString display     = res.key.isEmpty()
                                  ? QString("[%1] %2").arg(res.path).arg(res.value)
                                  : QString("%1: %2").arg(res.key).arg(res.value);

        item->setText(display);
        item->setData(res.path, SearchResultDelegate::kUserRolePath);
        item->setData(res.key, SearchResultDelegate::kUserRoleKey);
        item->setData(res.value, SearchResultDelegate::kUserRoleVal);
        item->setData(QString(res.type), SearchResultDelegate::kUserRoleType);

        item->setToolTip(res.path);
        m_results_model->appendRow(item);
    }
    m_label_count->setText(tr("%1 results").arg(m_results_model->rowCount()));
}

void SearchPanel::onSearchFinished(bool success)
{
    m_progress_fill->hide();

    if (success) {
        m_label_count->setText(
            tr("%1 results").arg(m_results_model->rowCount()));
    }
    else {
        m_label_count->setText(tr("Cancelled"));
    }
}

void SearchPanel::onResultClicked(const QModelIndex& index)
{
    if (m_is_navigating) {
        qprintt << "[SearchPanel] Still navigating, ignore click";
        return;
    }

    QString path = index.data(SearchResultDelegate::kUserRolePath).toString();
    if (path.isEmpty())
        return;

    m_is_navigating = true;
    m_view->setEnabled(false);

    if (!m_navigator) {
        m_navigator = new PathNavigator(this);
        connect(m_navigator, &PathNavigator::navigationCompleted, this,
                &SearchPanel::onNavigationCompleted);
    }
    m_navigator->navigate(m_model_ref.data(), path);
}

void SearchPanel::onNavigationCompleted(NavigationError error,
                                        const QString& message)
{
    m_is_navigating = false;
    m_view->setEnabled(true);

    qprintt << "Navigation completed with error code:"
            << static_cast<int>(error) << "message:" << message;
    if (error == NavigationError::Success) {
        if (m_navigator) {
            emit targetResolved(m_navigator->currentIndex());
        }
    }
    else {
        emit navigationFailed(message);
    }
}

void SearchPanel::clear()
{
    cancelSearch();
    m_results_model->clear();
    m_results_model->setHorizontalHeaderLabels({tr("Results")});
    m_label_query->clear();
    m_label_count->clear();
    m_progress_fill->hide();
    m_btn_cancel->hide();
    m_model_ref = nullptr;
}

void SearchPanel::updateDPR(qreal r)
{
    m_dpr = r;
    m_banner->setFixedHeight(32 * r);
    if (auto* lay = m_banner->findChild<QHBoxLayout*>()) {
        lay->setContentsMargins(12 * r, 0, 4 * r, 0);
        lay->setSpacing(10 * r);
    }

    QFont f1 = font();
    f1.setPixelSize(12 * r);
    f1.setBold(true);
    QFont f2 = font();
    f2.setPixelSize(11 * r);
    m_label_query->setFont(f1);
    m_label_count->setFont(f2);

    m_btn_cancel->setFixedSize(25 * r, 25 * r);
    m_btn_cancel->setIconSize(QSize(16 * r, 16 * r));

    m_view->doItemsLayout();

    reapplyStyles();
}

void SearchPanel::updateTheme(bool isDarkMode)
{
    m_isDarkMode = isDarkMode;
    reapplyStyles();
}

void SearchPanel::reapplyStyles()
{
    using namespace jtv::ui::Colors;

    // 0. Container background
    setStyleSheet(QString("QWidget#searchPanel { background-color: %1; }")
                      .arg(m_isDarkMode ? DarkBG : LightBG));

    // 1. Container and View style
    m_view->setStyleSheet(
        QString(g_qss_search_list).arg(m_isDarkMode ? DarkBG : LightBG));

    // 2. Banner style
    m_banner->setStyleSheet(
        QString(g_qss_search_banner)
            .arg(m_isDarkMode ? "#1E1E1E" : "#F5F5F5")  // BG
            .arg(m_isDarkMode ? DarkBorder : LightBorder)
            .arg(m_isDarkMode ? DarkText : LightText)
            .arg(qRound(2 * m_dpr))  //  QProgressBar height
            .arg(m_isDarkMode ? DarkTextDim : LightTextDim)
            .arg(m_isDarkMode ? DarkText : LightText)
            .arg(qRound(16 * m_dpr))  // FontSize
            .arg(qRound(4 * m_dpr))   // BtnPadding
    );

    m_progress_fill->setStyleSheet(QString("background-color: %1;")
                                       .arg(m_isDarkMode
                                                ? "rgba(2, 136, 209, 40)"
                                                : "rgba(2, 136, 209, 25)"));

    // 3. Palette (Base for all children)
    QPalette p = palette();
    QColor textC(m_isDarkMode ? DarkText : LightText);
    QColor dimC(m_isDarkMode ? DarkTextDim : LightTextDim);
    QColor bgC(m_isDarkMode ? DarkBG : LightBG);

    p.setColor(QPalette::Window,
               QColor(m_isDarkMode ? DarkSurface : LightSurface));
    p.setColor(QPalette::WindowText, textC);
    p.setColor(QPalette::Base, bgC);
    p.setColor(QPalette::Text, textC);
    p.setColor(QPalette::PlaceholderText, dimC);
    setPalette(p);

    m_label_query->setPalette(p);
    m_label_count->setPalette(p);
    m_view->setPalette(p);

    // 4. Icons
    m_btn_cancel->setIcon(
        jtv::ui::svgIcon(jtv::ui::g_svg_close,
                         m_isDarkMode ? DarkTextDim : LightTextDim, 16, m_dpr));
}
