#include "searchpanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include "../common.h"
#include "../jsontreemodel.h"
#include "searchresultdelegate.h"

SearchPanel::SearchPanel(QWidget* parent) : QWidget(parent)
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
    m_banner->setFixedHeight(32);
    m_banner->setStyleSheet(R"(
        QWidget#searchBanner { 
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1A237E, stop:1 #121858);
            border-bottom: 1px solid #303F9F; 
        }
        QLabel { color: #E8EAF6; font-weight: bold; }
        QProgressBar { 
            border: none; background: #283593; height: 4px; border-radius: 2px;
        }
        QProgressBar::chunk { background-color: #448AFF; border-radius: 2px; }
        QPushButton { color: #C5CAE9; font-size: 16px; font-weight: bold; border: none; background: transparent; }
        QPushButton:hover { color: #FFFFFF; }
    )");

    auto* bannerLay = new QHBoxLayout(m_banner);
    bannerLay->setContentsMargins(12, 0, 8, 0);
    bannerLay->setSpacing(10);

    m_label_query = new QLabel(this);
    m_label_query->setStyleSheet("font-size: 12px;");
    bannerLay->addWidget(m_label_query, 1);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setTextVisible(false);
    m_progress->setFixedWidth(80);
    m_progress->setFixedHeight(4);
    m_progress->hide();
    bannerLay->addWidget(m_progress);

    m_label_count = new QLabel(this);
    m_label_count->setStyleSheet("font-size: 11px; color: #C5CAE9;");
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
    m_view->setStyleSheet(
        "QListView { background-color: #121212; border: none; }");
    m_view->setModel(m_results_model);
    m_view->setItemDelegate(new SearchResultDelegate(this));
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mainLay->addWidget(m_view);

    connect(m_view, &QListView::clicked, this, &SearchPanel::onResultClicked);
    connect(m_btn_cancel, &QPushButton::clicked, this,
            &SearchPanel::cancelSearch);
    connect(m_btn_cancel, &QPushButton::clicked, this,
            &SearchPanel::cancelRequested);
}

SearchPanel::~SearchPanel()
{
    cancelSearch();
}

void SearchPanel::startSearch(JsonTreeModel* model,
                              std::shared_ptr<JsonViewerStrategy> strategy,
                              const SearchQuery& query)
{
    cancelSearch();
    m_model_ref = model;

    m_results_model->clear();
    m_results_model->setHorizontalHeaderLabels({tr("Results")});

    m_label_query->setText(tr("Searching: %1").arg(query.text));
    m_label_count->setText(tr("Searching..."));
    m_progress->setValue(0);
    m_progress->show();
    m_btn_cancel->show();
    show();

    // Create background thread with no parent for true async cleanup
    auto* thread    = new JTVThread;
    m_search_thread = thread;

    // Safety: hold strategy to keep memory alive
    auto* worker    = new SearchWorker(strategy, query);
    m_search_worker = worker;

    // Chained cleanup
    connect(worker, &QObject::destroyed, thread, &QObject::deleteLater);

    connect(this, &QObject::destroyed, [worker, thread]() {
        if (thread) {
            thread->requestInterruption();
            thread->quit();
        }
    });

    connect(thread, &QThread::started, worker, &SearchWorker::process);
    connect(worker, &SearchWorker::resultsFound, this,
            &SearchPanel::onResultsFound);
    connect(worker, &SearchWorker::progressUpdated, m_progress,
            &QProgressBar::setValue);
    connect(worker, &SearchWorker::finished, this,
            &SearchPanel::onSearchFinished);

    worker->moveToThread(thread);
    thread->start();
}

void SearchPanel::cancelSearch()
{
    if (m_search_thread) {
        if (m_search_thread->isRunning()) {
            m_search_thread->requestInterruption();
            m_search_thread->quit();
        }
        // m_search_thread will be nulled by QPointer when deleteLater is
        // processed, but we null it now to prevent double-cancel logic on next
        // call.
        m_search_thread = nullptr;
    }
    m_search_worker = nullptr;
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
    if (!path.isEmpty() && m_model_ref) {
        m_navigator->navigate(m_model_ref, path);
    }
}

void SearchPanel::onNavigationCompleted(NavigationError error,
                                        const QString& message)
{
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
