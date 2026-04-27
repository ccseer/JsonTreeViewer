#pragma once

#include <QModelIndex>
#include <QPointer>
#include <QVector>
#include <QWidget>

#include "../common.h"
#include "pathnavigator.h"
#include "searchworker.h"

class QLabel;
class QListView;
class QProgressBar;
class QPushButton;
class QStandardItemModel;
class JTVThread;
class JsonTreeModel;
class JsonViewerStrategy;

class SearchPanel : public QWidget {
    Q_OBJECT
public:
    explicit SearchPanel(QWidget* parent = nullptr);
    ~SearchPanel() override;

    void startSearch(JsonTreeModel* model,
                     std::shared_ptr<JsonViewerStrategy> strategy,
                     const SearchQuery& query);
    void cancelSearch();
    void updateTheme(bool isDark);
    void clear();
    void updateDPR(qreal r);

    QListView* listView() const
    {
        return m_view;
    }

signals:
    void targetResolved(const QModelIndex& index);
    void navigationFailed(const QString& msg);
    void cancelRequested();

private slots:
    void onResultsFound(const QVector<SearchResult>& results);
    void onSearchFinished(bool success);
    void onResultClicked(const QModelIndex& index);
    void onNavigationCompleted(NavigationError error, const QString& message);

private:
    QWidget* m_banner;
    QLabel* m_label_query;
    QLabel* m_label_count;
    QProgressBar* m_progress;
    QPushButton* m_btn_cancel;
    QListView* m_view;

    // Encapsulated Logic
    QStandardItemModel* m_results_model;
    QPointer<JTVThread> m_search_thread;
    SearchWorker* m_search_worker = nullptr;
    PathNavigator* m_navigator    = nullptr;
    QPointer<JsonTreeModel> m_model_ref;
};
