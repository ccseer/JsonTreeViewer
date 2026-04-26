#pragma once

#include <QModelIndex>
#include <QObject>
#include <QThread>
#include <QVector>
#include <memory>

#include "common.h"

class JsonTreeItem;
class JsonViewerStrategy;

/**
 * @brief Custom QThread that automatically cleans up in destructor
 *
 * This class ensures that quit() and wait() are called in the destructor.
 * The thread will finish cleanly without blocking indefinitely.
 *
 * IMPORTANT: Always use deleteLater() instead of direct delete to ensure
 * proper cleanup through Qt's event loop.
 */
class JTVThread : public QThread {
    Q_OBJECT

    // Hide run() to prevent direct override - use worker pattern instead
    using QThread::run;

public:
    explicit JTVThread(QObject* parent = nullptr) : QThread(parent) {}

    ~JTVThread() override
    {
        quit();
        wait();
    }
};

/**
 * @brief Worker object for background JSON file loading
 *
 * This worker runs in a separate thread to load JSON files without
 * blocking the UI thread. It is completely independent of JsonTreeModel
 * and only handles pure data operations.
 *
 * Key design principles:
 * - No Model dependency: does not hold any JsonTreeModel pointer
 * - Returns data via shared_ptr for safe lifetime management
 * - Uses QScopeGuard to ensure automatic cleanup
 * - Checks for interruption at multiple points
 */
class LoadWorker : public QObject {
    Q_OBJECT

public:
    explicit LoadWorker(const QString& path, QObject* parent = nullptr);
    ~LoadWorker();

public slots:
    /**
     * @brief Performs the actual file loading in background thread
     *
     * This method:
     * 1. Selects appropriate strategy based on file size
     * 2. Initializes the strategy (reads file content)
     * 3. Creates root node with metadata
     * 4. Returns data via signal
     * 5. Automatically calls deleteLater() on completion (via QScopeGuard)
     *
     * Checks for interruption at multiple points and returns early if
     * interrupted.
     */
    void doLoad();

signals:
    /**
     * @brief Emitted when loading completes (success or failure)
     * @param root The root item (nullptr on failure)
     * @param strategy The strategy used for this file (nullptr on failure)
     * @param success Whether the load operation succeeded
     * @param elapsedMs Time taken in milliseconds
     */
    void loadCompleted(std::shared_ptr<JsonTreeItem> root,
                       std::shared_ptr<JsonViewerStrategy> strategy,
                       bool success,
                       qint64 elapsedMs);

private:
    QString m_path;
};

/**
 * @brief Worker object for async fetchMore operations
 *
 * This worker runs extractChildren() in a background thread to avoid
 * blocking the UI when expanding large nodes. It is completely independent
 * of JsonTreeModel and only handles pure data operations.
 *
 * Key design principles:
 * - No Model dependency: does not hold any JsonTreeModel pointer
 * - No UI object pointers: uses metadata instead of JsonTreeItem*
 * - Returns data via shared_ptr for safe lifetime management
 * - Uses QScopeGuard to ensure automatic cleanup
 * - Checks for interruption before and after extraction
 */
class FetchWorker : public QObject {
    Q_OBJECT

public:
    explicit FetchWorker(std::shared_ptr<JsonViewerStrategy> strategy,
                         const QString& parent_pointer,
                         quint64 byte_offset,
                         quint64 byte_length,
                         JsonTreeItem* parent_item,
                         const QModelIndex& parent_index,
                         FileMode file_mode,
                         int page_start             = -1,
                         int page_end               = -1,
                         quint32 cached_child_count = 0,
                         QObject* parent            = nullptr);
    ~FetchWorker();

public slots:
    /**
     * @brief Performs the actual children extraction in background thread
     *
     * This method:
     * 1. Checks for interruption
     * 2. Extracts children using metadata (not UI object pointers)
     * 3. Returns data via signal
     * 4. Automatically calls deleteLater() on completion (via QScopeGuard)
     *
     * If interrupted, cleans up extracted data and returns early.
     */
    void doFetch();

signals:
    /**
     * @brief Emitted when extraction completes
     * @param children The extracted children (ownership transferred via
     * shared_ptr)
     * @param parent_item The parent item pointer (for verification)
     * @param parent_index The parent model index
     * @param elapsedMs Time taken in milliseconds
     */
    void fetchCompleted(std::shared_ptr<QVector<JsonTreeItem*>> children,
                        JsonTreeItem* parent_item,
                        const QModelIndex& parent_index,
                        qint64 elapsedMs);

    /**
     * @brief Emitted periodically during extraction to report progress
     * @param dotCount Animation state (0-3 for dot count)
     * @param unused Reserved for future use
     */
    void progressUpdated(int dotCount, int unused);

private:
    // Paging helper methods
    int getPageSize() const;
    bool needsPaging(int child_count) const;
    QVector<JsonTreeItem*> createPagedChildren(int total_children);

    std::shared_ptr<JsonViewerStrategy> m_strategy;
    QString m_parent_pointer;
    quint64 m_byte_offset;
    quint64 m_byte_length;
    // For verification only, not dereferenced in worker
    JsonTreeItem* m_parent_item;
    QModelIndex m_parent_index;
    FileMode m_file_mode;  // File mode for paging logic
    int m_page_start;  // Page range for virtual pages (-1 = not a virtual page)
    int m_page_end;
    quint32 m_cached_child_count;  // Cached from main thread to avoid
                                   // cross-thread access
};
