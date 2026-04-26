#include "jsontreemodel.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QStringBuilder>
#include <QTimer>

#include "jsonnode.h"
#include "loadworker.h"
#include "logging.h"
#include "strategies/jsonstrategy.h"

using namespace simdjson;

#define qprintt qprint << "[Model]"

namespace {
constexpr auto g_type_obj = "Object";
constexpr auto g_type_arr = "Array";
// constexpr auto g_type_str  = "String";
// constexpr auto g_type_num  = "Number";
// constexpr auto g_type_bool = "Boolean";

enum ColumnIndex : uchar { CI_Key = 0, CI_Value = 1, CI_Count };
}  // namespace

//////////////////////////////////////////////
JsonTreeModel::JsonTreeModel(QObject* parent)
    : QAbstractItemModel(parent),
      m_root_item(new JsonTreeItem(
          "root", {}, static_cast<char>(ondemand::json_type::null)))
{
    qprintt << "SIMDJSON_VERSION" << SIMDJSON_VERSION;
    m_root_item->has_children = true;
}

JsonTreeModel::~JsonTreeModel()
{
    qprintt << "~JsonTreeModel: Cleaning up";

    // With shared_ptr and auto-cleanup chain, threads will clean themselves up
    // We just need to clear the queue and fetching set
    m_fetch_queue.clear();
    m_fetching_items.clear();

    // Safe to delete root immediately (or let shared_ptr handle it)
    // m_strategy will be destroyed when last shared_ptr reference is released
    // Don't delete m_root_item manually since it's managed by m_root_shared
    m_root_item = nullptr;
    m_root_shared.reset();

    qprintt << "~JsonTreeModel: Done (strategy refcount:"
            << (m_strategy ? m_strategy.use_count() : 0) << ")";
}

bool JsonTreeModel::load(const QString& path)
{
    qprintt << "=== [LOAD START ASYNC] ===" << path;

    m_load_start_time = QDateTime::currentMSecsSinceEpoch();

    auto* thread                = new JTVThread;
    QPointer<LoadWorker> worker = new LoadWorker(path);
    connect(this, &JsonTreeModel::destroyed, [worker, thread]() {
        // ui destoryed while worker is running
        if (worker) {
            qprintt
                << "[LOAD ASYNC] Model destroyed, requesting worker to stop";
            thread->requestInterruption();
        }
    });
    connect(worker, &QObject::destroyed, thread, &QObject::deleteLater);
    // Business logic
    connect(thread, &QThread::started, worker, &LoadWorker::doLoad);
    connect(worker, &LoadWorker::loadCompleted, this,
            &JsonTreeModel::onLoadCompleted);
    worker->moveToThread(thread);
    thread->start();
    return true;
}

void JsonTreeModel::onLoadCompleted(
    std::shared_ptr<JsonTreeItem> root,
    std::shared_ptr<JsonViewerStrategy> strategy,
    bool success,
    qint64 elapsedMs)
{
    if (!success) {
        qprintt << "[LOAD ASYNC] Load failed";
        emit loadFinished(false, elapsedMs);
        return;
    }

    qprintt << "[LOAD ASYNC] Completed in" << elapsedMs << "ms";

    // Update model in main thread
    beginResetModel();

    m_root_item = nullptr;
    m_root_shared.reset();
    m_root_shared = root;        // Keep shared_ptr alive
    m_root_item   = root.get();  // Raw pointer for quick access
    m_strategy    = strategy;    // shared_ptr assignment keeps strategy alive

    // Determine file mode based on strategy type
    switch (strategy->type()) {
    case StrategyType::Small:
        m_file_mode = FileMode::Small;
        break;
    case StrategyType::Medium:
        m_file_mode = FileMode::Medium;
        break;
    case StrategyType::Large:
        m_file_mode = FileMode::Large;
        break;
    case StrategyType::Extreme:
        m_file_mode = FileMode::Extreme;
        break;
    }

    endResetModel();

    qprintt << "=== [LOAD END] Total time:" << elapsedMs << "ms ===";
    emit loadFinished(true, elapsedMs);
}

QVariant JsonTreeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return {};
    }
    // if (role == Qt::FontRole) {
    //     auto item = static_cast<JsonTreeItem*>(index.internalPointer());
    //     if (index.column() == 1
    //         && (item->type == g_type_obj || item->type == g_type_arr)) {
    //         QFont font;
    //         font.setItalic(true);
    //         return font;
    //     }
    //     return {};
    // }

    if (role != Qt::DisplayRole) {
        return {};
    }
    auto item = static_cast<JsonTreeItem*>(index.internalPointer());
    if (index.column() == CI_Key) {
        return item->key;
    }
    if (index.column() == CI_Value) {
        return item->value;
    }
    return {};
}

Qt::ItemFlags JsonTreeModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant JsonTreeModel::headerData(int section,
                                   Qt::Orientation o,
                                   int role) const
{
    if (o == Qt::Horizontal && role == Qt::DisplayRole) {
        if (section == CI_Key) {
            return "Key";
        }
        else if (section == CI_Value) {
            return "Value";
        }
    }
    return {};
}

QModelIndex JsonTreeModel::index(int row,
                                 int column,
                                 const QModelIndex& parent) const
{
    auto parent_item = getItem(parent);
    if (row < 0 || row >= parent_item->children.size()) {
        return {};
    }
    auto child_item = parent_item->children.at(row);
    return createIndex(row, column, child_item);
}

QModelIndex JsonTreeModel::parent(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return {};
    }
    auto parent_item
        = static_cast<JsonTreeItem*>(index.internalPointer())->parent;
    if (parent_item == nullptr || parent_item == m_root_item) {
        return {};
    }
    JsonTreeItem* grand_parent = parent_item->parent;
    int row                    = 0;
    if (grand_parent) {
        row = grand_parent->children.indexOf(parent_item);
    }
    return createIndex(row, 0, parent_item);
}

int JsonTreeModel::rowCount(const QModelIndex& parent) const
{
    return getItem(parent)->children.size();
}

int JsonTreeModel::columnCount(const QModelIndex&) const
{
    return CI_Count;
}

bool JsonTreeModel::canFetchMore(const QModelIndex& parent) const
{
    JsonTreeItem* item = getItem(parent);
    if (!item || !item->has_children) {
        return false;
    }
    if (!m_strategy) {
        return false;
    }

    // Critical: if already fetching, return false to avoid duplicate triggers
    if (m_fetching_items.contains(item)) {
        // qprintt << "[canFetchMore] Already fetching:" << item->key;
        return false;
    }

    // Only unfetched and not-currently-fetching items can fetch
    return item->children.isEmpty();
}

void JsonTreeModel::fetchMore(const QModelIndex& parent)
{
    if (!canFetchMore(parent)) {
        return;
    }

    JsonTreeItem* item = getItem(parent);
    if (!item || !item->has_children) {
        qprint_err;
        return;
    }

    qprintt << "=== [FETCH START ASYNC] ===" << item->key
            << "offset:" << item->byte_offset;

    // Check if this is the first fetch (root children)
    if (item == m_root_item && !m_waiting_for_first_fetch) {
        m_waiting_for_first_fetch = true;
        qprintt << "[FETCH] This is the first fetch (root children)";
    }

    beginInsertRows(parent, 0, 0);
    auto* loadingItem   = new JsonTreeItem("Loading...", "", 0);
    loadingItem->parent = item;
    item->children.append(loadingItem);
    endInsertRows();

    qprintt << "[FETCH] Loading placeholder inserted";
    fetchMoreAsync(parent);
}

JsonTreeItem* JsonTreeModel::getItem(const QModelIndex& index) const
{
    if (index.isValid()) {
        return static_cast<JsonTreeItem*>(index.internalPointer());
    }
    return m_root_item;
}

int JsonTreeModel::getPageSize(FileMode mode) const
{
    switch (mode) {
    case FileMode::Small:
        return 10000;
    case FileMode::Medium:
        return 1000;
    case FileMode::Large:
        return 500;
    case FileMode::Extreme:
        return 100;
    }
    return 1000;
}

bool JsonTreeModel::needsPaging(int child_count, FileMode mode) const
{
    return child_count > getPageSize(mode);
}

QVector<JsonTreeItem*> JsonTreeModel::createPagedChildren(
    JsonTreeItem* parent_item, int total_children)
{
    QVector<JsonTreeItem*> paged_children;
    int page_size = getPageSize(m_file_mode);
    int total     = total_children;

    // Create virtual page nodes
    for (int start = 0; start < total; start += page_size) {
        int end = qMin(start + page_size - 1, total - 1);

        // Create virtual page node
        QString page_key = QString("[%1..%2]").arg(start).arg(end);
        JsonTreeItem* page_node
            = new JsonTreeItem(page_key,
                               parent_item->pointer,  // Same pointer as parent
                               parent_item->type,     // Same type as parent
                               QString(), parent_item);

        page_node->is_virtual_page = true;
        page_node->page_start      = start;
        page_node->page_end        = end;
        page_node->has_children    = true;
        page_node->children_loaded = false;
        page_node->child_count     = end - start + 1;

        paged_children.append(page_node);
    }

    // No original children to delete since we just count them now

    return paged_children;
}

bool JsonTreeModel::hasChildren(const QModelIndex& parent) const
{
    auto parent_item = getItem(parent);
    if (!parent_item) {
        return false;
    }
    return parent_item->has_children;
}

///////////////////////////////////////////////////////////////////////////////
TreeFilterProxyModel::TreeFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setRecursiveFilteringEnabled(true);
}

void TreeFilterProxyModel::updateFilter(const QString& text)
{
    m_filter_text = text;

    invalidate();
}

bool TreeFilterProxyModel::filterAcceptsRow(int row,
                                            const QModelIndex& parent) const
{
    if (m_filter_text.isEmpty()) {
        return true;
    }
    for (int i = 0; i < CI_Count; ++i) {
        auto index = sourceModel()->index(row, i, parent);
        if (index.isValid()
            && index.data().toString().contains(m_filter_text,
                                                Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

// Copy operations implementation
QString JsonTreeModel::getKey(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return QString();
    }

    JsonTreeItem* item = getItem(index);
    return item ? item->key : QString();
}

QString JsonTreeModel::getValue(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return QString();
    }

    JsonTreeItem* item = getItem(index);
    return item ? item->value : QString();
}

QString JsonTreeModel::getPath(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return QString();
    }

    JsonTreeItem* item = getItem(index);
    return item ? item->pointer : QString();
}

QString JsonTreeModel::getKeyValue(const QModelIndex& index,
                                   bool* success,
                                   QString* errorMsg) const
{
    if (success)
        *success = false;

    if (!index.isValid()) {
        if (errorMsg)
            *errorMsg = "Invalid index";
        return QString();
    }

    JsonTreeItem* item = getItem(index);
    if (!item) {
        if (errorMsg)
            *errorMsg = "Invalid item";
        return QString();
    }

    if (item->is_virtual_page) {
        if (errorMsg)
            *errorMsg = "Cannot copy a page node.";
        return QString();
    }

    if (item->type == '[') {
        if (errorMsg)
            *errorMsg = "Copy Key:Value is not supported for array nodes.";
        return QString();
    }

    QString valueStr;
    if (!item->has_children) {
        if (item->type == 's') {
            QJsonDocument tmp(QJsonArray{item->value});
            QByteArray arr = tmp.toJson(QJsonDocument::Compact);
            // arr is ["value"], strip outer brackets
            valueStr = QString::fromUtf8(arr.mid(1, arr.size() - 2));
        }
        else {
            valueStr = item->value;
        }
    }
    else {
        QString subtreeErr;
        bool subtreeOk = false;
        valueStr       = getSubtree(index, &subtreeOk, &subtreeErr);
        if (!subtreeOk) {
            if (errorMsg)
                *errorMsg = subtreeErr;
            return QString();
        }
    }

    if (success)
        *success = true;
    return QString("\"%1\": %2").arg(item->key, valueStr);
}

QString JsonTreeModel::getSubtree(const QModelIndex& index,
                                  bool* success,
                                  QString* errorMsg) const
{
    if (success)
        *success = false;

    if (!index.isValid()) {
        if (errorMsg)
            *errorMsg = "Invalid index";
        return QString();
    }

    JsonTreeItem* item = getItem(index);
    if (!item) {
        if (errorMsg)
            *errorMsg = "Invalid item";
        return QString();
    }

    if (item->is_virtual_page) {
        if (errorMsg)
            *errorMsg = "Cannot copy a page node. Expand a real item instead.";
        return QString();
    }

    // Check size limit first (applies to both scalar and container types)
    constexpr quint64 MAX_SUBTREE_SIZE = 10 * 1024 * 1024;  // 10 MB limit

    if (item->byte_length > MAX_SUBTREE_SIZE) {
        if (errorMsg) {
            *errorMsg
                = QString(
                      "Subtree too large (%1 MB). Maximum allowed is 10 MB.")
                      .arg(item->byte_length / (1024.0 * 1024.0), 0, 'f', 2);
        }
        return QString();
    }

    // Check if this is a scalar value (no children)
    if (!item->has_children) {
        if (success)
            *success = true;
        return item->value;
    }

    if (!m_strategy) {
        if (errorMsg)
            *errorMsg = "No strategy loaded";
        return QString();
    }

    const char* dataPtr = m_strategy->dataPtr();
    if (!dataPtr) {
        if (errorMsg)
            *errorMsg = "Cannot access data";
        return QString();
    }

    const char* subtree_start = dataPtr + item->byte_offset;
    size_t chunk              = std::min<size_t>(item->byte_length,
                                                 m_strategy->dataSize() - item->byte_offset);

    QByteArray jsonData(subtree_start, chunk);
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

    if (doc.isNull()) {
        if (errorMsg)
            *errorMsg = QString("JSON parse error at offset %1: %2")
                            .arg(parseError.offset)
                            .arg(parseError.errorString());
        return QString();
    }

    if (success)
        *success = true;
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

void JsonTreeModel::fetchMoreAsync(const QModelIndex& parent)
{
    JsonTreeItem* item = getItem(parent);

    // Check if already in queue or being processed
    if (m_fetching_items.contains(item)) {
        qprintt << "[FETCH ASYNC] Item already queued or processing:"
                << item->key;
        return;
    }

    // Mark as queued
    m_fetching_items.insert(item);

    // Add to queue
    m_fetch_queue.enqueue({item, parent});
    qprintt << "[FETCH ASYNC] Queued:" << item->key
            << "Queue size:" << m_fetch_queue.size();

    // Process queue (will start if no fetch is active)
    processFetchQueue();
}

void JsonTreeModel::processFetchQueue()
{
    // Check if already processing
    if (m_fetch_in_progress) {
        qprintt << "[FETCH ASYNC] Fetch already in progress, will process "
                   "queue later";
        return;
    }

    // Get next item to process
    if (m_fetch_queue.isEmpty()) {
        qprintt << "[FETCH ASYNC] Queue empty, nothing to process";
        return;
    }

    auto request       = m_fetch_queue.dequeue();
    JsonTreeItem* item = request.item;
    QModelIndex parent = request.parent;

    qprintt << "[FETCH ASYNC] Processing:" << item->key
            << "Queue size:" << m_fetch_queue.size();

    // Mark as in progress
    m_fetch_in_progress = true;

    // Check if this is a virtual page
    int page_start        = -1;
    int page_end          = -1;
    QString fetch_pointer = item->pointer;
    quint64 fetch_offset  = item->byte_offset;
    quint64 fetch_length  = item->byte_length;

    if (item->is_virtual_page && item->parent) {
        // For virtual pages, use parent's pointer and pass page range
        page_start    = item->page_start;
        page_end      = item->page_end;
        fetch_pointer = item->parent->pointer;
        fetch_offset  = item->parent->byte_offset;
        fetch_length  = item->parent->byte_length;
        qprintt << "[FETCH ASYNC] Virtual page [" << page_start << ".."
                << page_end << "]";
    }

    // Create background thread with no parent for true async cleanup
    auto* thread                 = new JTVThread;
    QPointer<FetchWorker> worker = new FetchWorker(
        m_strategy, fetch_pointer, fetch_offset, fetch_length, item, parent,
        static_cast<int>(m_file_mode), page_start, page_end,
        item->child_count);  // Pass cached count to avoid cross-thread access
    connect(this, &QObject::destroyed, [worker, thread]() {
        if (worker) {
            qprintt
                << "[FETCH ASYNC] Model destroyed, requesting worker to stop";
            thread->requestInterruption();
        }
    });
    connect(worker, &QObject::destroyed, thread, &QObject::deleteLater);
    // Business logic
    connect(thread, &QThread::started, worker, &FetchWorker::doFetch);
    connect(worker, &FetchWorker::fetchCompleted, this,
            &JsonTreeModel::onFetchCompleted);
    connect(worker, &FetchWorker::progressUpdated, this,
            &JsonTreeModel::onFetchProgress);
    worker->moveToThread(thread);
    thread->start();
}

void JsonTreeModel::onFetchCompleted(
    std::shared_ptr<QVector<JsonTreeItem*>> children,
    JsonTreeItem* parent_item,
    const QModelIndex& parent_index,
    qint64 elapsedMs)
{
    // Verify parent_item is still in our fetching set
    if (!m_fetching_items.contains(parent_item)) {
        qprintt << "[FETCH ASYNC] Parent item no longer in fetching set, "
                   "discarding results";
        return;
    }

    // Remove from fetching set first
    m_fetching_items.remove(parent_item);

    // Rebuild a valid QModelIndex from parent_item
    // The old parent_index might be invalid if model was reset
    QModelIndex valid_parent_index;
    if (parent_item == m_root_item) {
        valid_parent_index = QModelIndex();  // Root has invalid index
    }
    else {
        // Find the row of parent_item in its parent's children
        JsonTreeItem* grandparent = parent_item->parent;
        if (grandparent) {
            int row = grandparent->children.indexOf(parent_item);
            if (row >= 0) {
                valid_parent_index = createIndex(row, 0, parent_item);
            }
            else {
                qprintt
                    << "[FETCH ASYNC] Parent item not found in grandparent's "
                       "children, discarding results";
                cleanupFetchState();
                processFetchQueue();
                return;
            }
        }
        else {
            qprintt << "[FETCH ASYNC] Parent item has no grandparent, "
                       "discarding results";
            cleanupFetchState();
            processFetchQueue();
            return;
        }
    }

    qprintt << "[FETCH ASYNC] Completed:" << parent_item->key << "in"
            << elapsedMs << "ms," << children->size() << "items";

    // Remove "Loading..." placeholder if exists
    if (!parent_item->children.isEmpty()
        && parent_item->children.first()->isLoadingPlaceholder()) {
        beginRemoveRows(valid_parent_index, 0, 0);
        delete parent_item->children.takeFirst();
        endRemoveRows();
    }

    // Insert actual children
    int count = children->size();
    if (count > 0) {
        beginInsertRows(valid_parent_index, 0, count - 1);

        // Set parent relationship
        for (auto* child : *children) {
            child->parent = parent_item;
        }
        parent_item->children        = *children;
        parent_item->children_loaded = true;

        // Cache the child count for future paging decisions
        // This avoids needing to call countChildren() again
        if (parent_item->child_count == 0) {
            parent_item->child_count = count;
        }

        // Model 已经"认领"了这些指针，现在它们由 parent->children 管理
        // 清空后，当 shared_ptr 析构时，自定义删除器看到的是空容器
        // 不会误删已经交给 Model 管理的节点
        children->clear();

        endInsertRows();
    }

    // Check if this was the first fetch (root children)
    if (m_waiting_for_first_fetch && parent_item == m_root_item) {
        m_waiting_for_first_fetch = false;
        qint64 total_time
            = QDateTime::currentMSecsSinceEpoch() - m_load_start_time;
        qprintt << "=== [FIRST FETCH END] Total time (including first fetch):"
                << total_time << "ms ===";
        emit firstFetchCompleted(total_time);
    }

    // Cleanup current fetch
    cleanupFetchState();
    // Process next item in queue
    processFetchQueue();
}

void JsonTreeModel::onFetchProgress(int dotCount, int unused)
{
    Q_UNUSED(dotCount);
    Q_UNUSED(unused);
    // Progress updates from worker - currently not used for animation
    // Could be used to update "Loading..." text in the future
}

void JsonTreeModel::cleanupFetchState()
{
    qprintt << "[FETCH ASYNC] Cleaning up fetch state";

    // Clear in-progress flag
    m_fetch_in_progress = false;

    // Note: Don't clear m_fetching_items here, items are removed when dequeued
}