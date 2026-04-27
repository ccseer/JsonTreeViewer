#include "searchpanel.h"

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
    auto* mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(0, 0, 0, 0);
    mainLay->setSpacing(0);

    m_results_model = new QStandardItemModel(this);
    m_navigator     = new PathNavigator(this);
    connect(m_navigator, &PathNavigator::navigationCompleted, this,
            &SearchPanel::onNavigationCompleted);

    // Banner UI
    m_banner = new QWidget(this);
    m_banner->setObjectName("searchBanner");
    m_banner->setStyleSheet(g_qss_search_banner);

    auto* bannerLay = new QHBoxLayout(m_banner);
    bannerLay->setContentsMargins(12, 0, 8, 0);
    bannerLay->setSpacing(10);

    m_label_query = new QLabel(this);
    m_label_query->setStyleSheet(g_qss_label_query);
    bannerLay->addWidget(m_label_query, 1);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setTextVisible(false);
    m_progress->hide();
    bannerLay->addWidget(m_progress);

    m_label_count = new QLabel(this);
    m_label_count->setStyleSheet(g_qss_label_count);
    bannerLay->addWidget(m_label_count);

    m_btn_cancel = new QPushButton("×", this);
    m_btn_cancel->setToolTip(tr("Cancel Search"));
    m_btn_cancel->setFixedWidth(24);
    m_btn_cancel->hide();
    bannerLay->addWidget(m_btn_cancel);

    mainLay->addWidget(m_banner);

    // List UI
    m_view = new QListView(this);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setStyleSheet(g_qss_search_list);
    m_view->setModel(m_results_model);
    m_view->setItemDelegate(new SearchResultDelegate(this));
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
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
    m_progress->setValue(0);
    m_progress->show();
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
    connect(this, &SearchPanel::cancelRequested, worker, [worker, thread]() {
        if (worker) {
            qprintt << "[SEARCH ASYNC] Requesting worker to stop";
            thread->requestInterruption();
        }
        else {
            qprintt << "[SEARCH ASYNC] Worker already destroyed, cannot cancel";
        }
    });
    // Business logic
    connect(worker, &SearchWorker::resultsFound, this,
            &SearchPanel::onResultsFound);
    connect(worker, &SearchWorker::progressUpdated, m_progress,
            &QProgressBar::setValue);
    connect(worker, &SearchWorker::finished, this,
            &SearchPanel::onSearchFinished);
    connect(thread, &QThread::started, worker, &SearchWorker::process);

    worker->moveToThread(thread);
    thread->start();
}

void SearchPanel::cancelSearch()
{
    emit cancelRequested();
    m_btn_cancel->hide();
    m_progress->hide();
}

void SearchPanel::onResultsFound(const QVector<SearchResult>& results)
{
    for (const auto& res : results) {
        QStandardItem* item = new QStandardItem();
        QString display     = res.key.isEmpty()
                                  ? QString("[%1] %2").arg(res.path).arg(res.value)
                                  : QString("%1: %2").arg(res.key).arg(res.value);

        item->setText(display);
        item->setData(res.path, Qt::UserRole);
        item->setToolTip(res.path);
        m_results_model->appendRow(item);
    }
    m_label_count->setText(tr("%1 results").arg(m_results_model->rowCount()));
}

void SearchPanel::onSearchFinished(bool success)
{
    m_btn_cancel->hide();
    m_progress->hide();
    if (success) {
        m_label_count->setText(
            tr("%1 results").arg(m_results_model->rowCount()));
    }
    else {
        m_label_count->setText(tr("Failed or cancelled"));
    }
}

void SearchPanel::onResultClicked(const QModelIndex& index)
{
    QString path = index.data(Qt::UserRole).toString();
    qprintt << "Result clicked: " << path;
    if (!path.isEmpty() && m_model_ref) {
        m_navigator->navigate(m_model_ref, path);
    }
}

void SearchPanel::onNavigationCompleted(NavigationError error,
                                        const QString& message)
{
    qprintt << "Navigation completed with error code:"
            << static_cast<int>(error) << "message:" << message;
    if (error == NavigationError::Success) {
        emit targetResolved(m_navigator->currentIndex());
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
    m_progress->hide();
    m_btn_cancel->hide();
    m_model_ref = nullptr;
}

void SearchPanel::updateDPR(qreal r)
{
    m_banner->setFixedHeight(32 * r);
    m_banner->layout()->setContentsMargins(12 * r, 0, 8 * r, 0);
    m_progress->setFixedWidth(80 * r);
    m_progress->setFixedHeight(4 * r);
    m_btn_cancel->setFixedWidth(24 * r);

    QFont f1 = font();
    f1.setPixelSize(12 * r);
    f1.setBold(true);
    QFont f2 = font();
    f2.setPixelSize(11 * r);
    m_label_query->setFont(f1);
    m_label_count->setFont(f2);
}

void SearchPanel::updateTheme(bool isDark)
{
    using namespace jtv::ui::Colors;

    // 1. Banner style
    m_banner->setStyleSheet(
        QString(g_qss_search_banner)
            .arg(isDark ? "#1A237E" : "#E8EAF6")  // stop0
            .arg(isDark ? "#121858" : "#C5CAE9")  // stop1
            .arg(isDark ? DarkBorder : LightBorder)
            .arg(isDark ? DarkText : LightText)
            .arg(isDark ? "#283593" : "#BDBDBD")  // progressBG
            .arg(Accent)                          // progressChunk
            .arg(isDark ? "#C5CAE9" : "#3F51B5")  // btnText
            .arg(isDark ? "#FFFFFF" : "#1A237E")  // btnHover
    );

    // 2. Labels
    m_label_query->setStyleSheet(
        QString(g_qss_label_query).arg(isDark ? DarkText : LightText));
    m_label_count->setStyleSheet(
        QString(g_qss_label_count).arg(isDark ? DarkTextDim : LightTextDim));

    // 3. View style
    m_view->setStyleSheet(
        QString(g_qss_search_list).arg(isDark ? DarkBG : LightBG));
}
