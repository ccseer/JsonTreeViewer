#pragma once

#include <QQueue>
#include <QSortFilterProxyModel>

#include "strategies/jsonstrategy.h"

class JsonTreeItem;
class JTVThread;

class JsonTreeModel : public QAbstractItemModel {
    Q_OBJECT
public:
    using CopyActions = JsonViewerStrategy::CopyActions;
    using CopyAction  = JsonViewerStrategy::CopyAction;

    enum class FileMode { Small, Medium, Large, Extreme };

    explicit JsonTreeModel(QObject* parent = nullptr);
    ~JsonTreeModel() override;

    bool load(const QString& path);
    Q_SIGNAL void loadFinished(bool success, qint64 elapsedMs);

    FileMode fileMode() const
    {
        return m_file_mode;
    }

    CopyActions supportedActions() const
    {
        return m_strategy ? m_strategy->supportedActions() : CopyActions{};
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section,
                        Qt::Orientation o,
                        int role) const override;
    QModelIndex index(int row,
                      int column,
                      const QModelIndex& parent = QModelIndex()) const override;

    bool hasChildren(const QModelIndex& parent) const override;
    QModelIndex parent(const QModelIndex& index) const override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& = QModelIndex()) const override;

    bool canFetchMore(const QModelIndex& parent) const override;
    void fetchMore(const QModelIndex& parent) override;

    // Copy operations
    QString getKey(const QModelIndex& index) const;
    QString getValue(const QModelIndex& index) const;
    QString getPath(const QModelIndex& index) const;
    QString getKeyValue(const QModelIndex& index,
                        bool* success     = nullptr,
                        QString* errorMsg = nullptr) const;
    QString getSubtree(const QModelIndex& index,
                       bool* success     = nullptr,
                       QString* errorMsg = nullptr) const;

private slots:
    // Slots for receiving data from background workers
    void onLoadCompleted(std::shared_ptr<JsonTreeItem> root,
                         std::shared_ptr<JsonViewerStrategy> strategy,
                         bool success,
                         qint64 elapsedMs);

    void onFetchCompleted(std::shared_ptr<QVector<JsonTreeItem*>> children,
                          JsonTreeItem* parent_item,
                          const QModelIndex& parent_index,
                          qint64 elapsedMs);

    void onFetchProgress(int dotCount, int unused);

private:
    JsonTreeItem* getItem(const QModelIndex& index) const;

    // Paging support
    int getPageSize(FileMode mode) const;
    bool needsPaging(int child_count, FileMode mode) const;
    QVector<JsonTreeItem*> createPagedChildren(JsonTreeItem* parent_item,
                                               int total_children);

    // Async fetchMore support - single thread with queue
    // Items currently being fetched or queued
    QSet<JsonTreeItem*> m_fetching_items;
    bool m_fetch_in_progress = false;  // Is a fetch currently running?

    struct FetchRequest {
        JsonTreeItem* item;
        QModelIndex parent;
    };
    QQueue<FetchRequest> m_fetch_queue;  // Queue of pending requests

    void fetchMoreAsync(const QModelIndex& parent);
    void processFetchQueue();  // Process next item in queue
    void cleanupFetchState();

    JsonTreeItem* m_root_item;
    // Keep root alive via shared_ptr
    std::shared_ptr<JsonTreeItem> m_root_shared;
    // shared_ptr for thread safety
    std::shared_ptr<JsonViewerStrategy> m_strategy;
    FileMode m_file_mode;

    // Legacy members - will be removed after full strategy integration
    simdjson::ondemand::parser m_parser;
    simdjson::padded_string m_json_data;
    simdjson::ondemand::document m_doc;
};

////////////////////////////////////////////////////////////
class TreeFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    TreeFilterProxyModel(QObject* parent = nullptr);

    void updateFilter(const QString& text);

protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override;
    QString m_filter_text;
};
