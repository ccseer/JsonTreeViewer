#include "loadworker.h"

#include <QFileInfo>
#include <QScopeGuard>

#include "jsonnode.h"
#include "logging.h"
#include "strategies/extremefilestrategy.h"
#include "strategies/jsonstrategy.h"
#include "strategies/largefilestrategy.h"
#include "strategies/mediumfilestrategy.h"
#include "strategies/smallfilestrategy.h"

#define qprintt qprint << "[LoadWorker]"

LoadWorker::LoadWorker(const QString& path, QObject* parent)
    : QObject(parent), m_path(path)
{
    qprintt << this;
}

LoadWorker::~LoadWorker()
{
    qprintt << "~" << this;
}

void LoadWorker::doLoad()
{
    qprintt << "LoadWorker::doLoad enter" << m_path;
    QScopeGuard _cleanup([this]() {
        qprintt << "LoadWorker::doLoad quit";
        this->deleteLater();
    });

    QElapsedTimer timer;
    timer.start();

    // 1. Check for interruption before starting
    if (QThread::currentThread()->isInterruptionRequested()) {
        qprintt << "[BG LOAD] Interrupted before start";
        return;
    }

    // 2. Get file size to select strategy
    QFileInfo fileInfo(m_path);
    if (!fileInfo.exists()) {
        qprintt << "[BG LOAD] File does not exist:" << m_path;
        emit loadCompleted(nullptr, nullptr, false, timer.elapsed());
        return;
    }

    qint64 fileSize = fileInfo.size();
    qprintt << "[BG LOAD] File size:" << fileSize << "bytes ("
            << (fileSize / 1024.0 / 1024.0) << "MB)";

    // 3. Select strategy based on file size using factory method
    auto strategy = JsonViewerStrategy::createStrategy(fileSize);

    // 4. Check for interruption before initialization
    if (QThread::currentThread()->isInterruptionRequested()) {
        qprintt << "[BG LOAD] Interrupted before initialize";
        return;
    }

    // 5. Initialize strategy (reads file content, may take time)
    qprintt << "[BG LOAD] Initializing strategy...";
    if (!strategy->initialize(m_path)) {
        qprintt << "[BG LOAD] Strategy initialization failed";
        emit loadCompleted(nullptr, nullptr, false, timer.elapsed());
        return;
    }

    // 6. Check for interruption after initialization
    if (QThread::currentThread()->isInterruptionRequested()) {
        qprintt << "[BG LOAD] Interrupted after initialize";
        return;
    }

    // 7. Create root node (pure data operation)
    auto root = std::make_shared<JsonTreeItem>(
        "root", "", 'o', QString(),
        nullptr  // Empty pointer for root per RFC 6901
    );

    // 8. Get root metadata from strategy
    QString root_pointer;
    quint64 byte_offset = 0;
    quint64 byte_length = 0;
    quint32 child_count = 0;
    strategy->getRootMetadata(root_pointer, byte_offset, byte_length,
                              child_count);

    root->pointer      = root_pointer;  // Should be empty string
    root->byte_offset  = byte_offset;
    root->byte_length  = byte_length;
    root->child_count  = child_count;
    root->has_children = (child_count > 0);
    qprintt << "[BG LOAD] Root metadata: pointer=" << root_pointer
            << ", offset=" << byte_offset << ", length=" << byte_length
            << ", children=" << child_count;

    // 9. Final check for interruption before emitting
    if (QThread::currentThread()->isInterruptionRequested()) {
        qprintt << "[BG LOAD] Interrupted before emit";
        return;
    }

    // 10. Send data to main thread
    qint64 elapsed = timer.elapsed();
    qprintt << "[BG LOAD] Completed successfully in" << elapsed << "ms";
    emit loadCompleted(root, strategy, true, elapsed);
}

#undef qprintt
#define qprintt qDebug() << "[FetchWorker]"

FetchWorker::FetchWorker(std::shared_ptr<JsonViewerStrategy> strategy,
                         const QString& parent_pointer,
                         quint64 byte_offset,
                         quint64 byte_length,
                         JsonTreeItem* parent_item,
                         const QModelIndex& parent_index,
                         int file_mode,
                         int page_start,
                         int page_end,
                         quint32 cached_child_count,
                         QObject* parent)
    : QObject(parent),
      m_strategy(strategy),
      m_parent_pointer(parent_pointer),
      m_byte_offset(byte_offset),
      m_byte_length(byte_length),
      m_parent_item(parent_item),
      m_parent_index(parent_index),
      m_file_mode(file_mode),
      m_page_start(page_start),
      m_page_end(page_end),
      m_cached_child_count(cached_child_count)
{
    qprintt << this;
}

FetchWorker::~FetchWorker()
{
    qprintt << "~" << this;
}

void FetchWorker::doFetch()
{
    // Use QScopeGuard to ensure automatic cleanup on function exit
    QScopeGuard cleanup([this]() {
        qprintt << "Cleaning up, calling deleteLater";
        this->deleteLater();
    });

    qprintt << "[FETCH ASYNC] Thread started for pointer:" << m_parent_pointer;
    QElapsedTimer timer;
    timer.start();

    // 1. Check for interruption before starting
    if (QThread::currentThread()->isInterruptionRequested()) {
        qprintt << "[FETCH ASYNC] Interrupted before start";
        return;
    }

    // 2. Emit initial progress
    emit progressUpdated(0, 0);

    // 3. Extract children using metadata (no UI object pointers)
    qprintt << "[FETCH ASYNC] Extracting children: offset=" << m_byte_offset
            << ", length=" << m_byte_length;

    // Use custom deleter to ensure cleanup on interruption
    auto children = std::shared_ptr<QVector<JsonTreeItem*>>(
        new QVector<JsonTreeItem*>(), [](QVector<JsonTreeItem*>* vec) {
            qDeleteAll(*vec);
            delete vec;
        });

    // Check if this is a virtual page expansion
    if (m_page_start >= 0 && m_page_end >= 0) {
        // Virtual page: extract specific range, no paging needed
        qprintt << "[FETCH ASYNC] Expanding virtual page [" << m_page_start
                << ".." << m_page_end << "]";
        *children = m_strategy->extractChildren(m_parent_pointer, m_byte_offset,
                                                m_byte_length, m_page_start,
                                                m_page_end);
    }
    else {
        // Normal node: check if we might need paging based on cached
        // child_count Use cached value from main thread to avoid cross-thread
        // access
        quint32 child_count = m_cached_child_count;

        if (child_count > 0 && !needsPaging(child_count)) {
            // Small node with known count - extract directly without counting
            // again
            qprintt << "[FETCH ASYNC] Small node (cached count=" << child_count
                    << "), extracting directly";
            *children = m_strategy->extractChildren(
                m_parent_pointer, m_byte_offset, m_byte_length);
        }
        else {
            // Unknown count or potentially large - count first to decide on
            // paging
            child_count = m_strategy->countChildren(
                m_parent_pointer, m_byte_offset, m_byte_length);
            qprintt << "[FETCH ASYNC] Counted" << child_count << "children";

            qprintt << "[FETCH ASYNC] child_count=" << child_count
                    << ", needsPaging=" << needsPaging(child_count);

            // Apply paging logic if needed
            if (needsPaging(child_count)) {
                *children = createPagedChildren(child_count);
            }
            else {
                *children = m_strategy->extractChildren(
                    m_parent_pointer, m_byte_offset, m_byte_length);
            }
        }
    }

    // 4. Check for interruption after extraction
    if (QThread::currentThread()->isInterruptionRequested()) {
        qprintt << "[FETCH ASYNC] Interrupted after extraction, cleaning up"
                << children->size() << "items";
        // Custom deleter will automatically clean up
        return;
    }

    qint64 elapsed = timer.elapsed();
    qprintt << "[FETCH ASYNC] Extract completed in" << elapsed << "ms,"
            << children->size() << "items";

    // 5. Send data to main thread
    emit fetchCompleted(children, m_parent_item, m_parent_index, elapsed);
}

int FetchWorker::getPageSize() const
{
    switch (m_file_mode) {
    case 0:  // Small
        return 10000;
    case 1:  // Medium
        return 1000;
    case 2:  // Large
        return 500;
    case 3:  // Extreme
        return 100;
    }
    return 1000;
}

bool FetchWorker::needsPaging(int child_count) const
{
    return child_count > getPageSize();
}

QVector<JsonTreeItem*> FetchWorker::createPagedChildren(int total_children)
{
    QVector<JsonTreeItem*> paged_children;
    int page_size = getPageSize();
    int total     = total_children;

    // Create virtual page nodes
    for (int start = 0; start < total; start += page_size) {
        int end = qMin(start + page_size - 1, total - 1);

        // Create virtual page node
        QString page_key        = QString("[%1..%2]").arg(start).arg(end);
        JsonTreeItem* page_node = new JsonTreeItem(
            page_key, m_parent_pointer,
            static_cast<char>(simdjson::ondemand::json_type::array));

        page_node->is_virtual_page = true;
        page_node->page_start      = start;
        page_node->page_end        = end;
        page_node->has_children    = true;
        page_node->parent          = m_parent_item;
        page_node->byte_offset     = m_byte_offset;
        page_node->byte_length     = m_byte_length;

        paged_children.append(page_node);
    }

    qprintt << "[FETCH ASYNC] Created" << paged_children.size()
            << "virtual pages for" << total_children << "items";

    return paged_children;
}
