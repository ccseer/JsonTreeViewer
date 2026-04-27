#include "jsontreemodel.h"

#include <QApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPointer>
#include <QRegularExpression>
#include <QStringBuilder>
#include <QSvgRenderer>
#include <QTimer>
#include <memory>

#include "config.h"
#include "jsonnode.h"
#include "loadworker.h"
#include "strategies/jsonstrategy.h"
#include "style_assets.h"

using namespace simdjson;
using namespace jtv::ui;

#define qprintt qprint << "[Model]"

namespace {

enum ColumnIndex : uchar { CI_Key = 0, CI_Value = 1, CI_Count };

// Color detection helper
QColor parseColorValue(const QString& value)
{
    if (value.isEmpty())
        return {};

    // #RGB or #RRGGBB
    static QRegularExpression hexColor(R"(^#([0-9A-Fa-f]{3}|[0-9A-Fa-f]{6})$)");
    auto match = hexColor.match(value);
    if (match.hasMatch()) {
        return QColor(value);
    }

    // rgb(r, g, b) or rgba(r, g, b, a)
    static QRegularExpression rgbColor(
        R"(^rgba?\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*(?:,\s*([\d.]+)\s*)?\)$)");
    match = rgbColor.match(value);
    if (match.hasMatch()) {
        int r = match.captured(1).toInt();
        int g = match.captured(2).toInt();
        int b = match.captured(3).toInt();
        if (r > 255 || g > 255 || b > 255)
            return {};

        if (match.lastCapturedIndex() == 4) {
            // rgba
            double a = match.captured(4).toDouble();
            return QColor(r, g, b, qRound(a * 255));
        }
        return QColor(r, g, b);
    }

    return {};
}

// Create color preview icon
QPixmap createColorPreview(const QColor& color, qreal dpr, int size)
{
    const int phys = qRound(size * dpr);
    QPixmap pixmap(phys, phys);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw background if transparent (checkerboard)
    if (color.alpha() < 255) {
        painter.save();
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::white);
        painter.drawRoundedRect(QRectF(0, 0, size, size), 2, 2);
        painter.setBrush(QColor(200, 200, 200));
        painter.drawRect(0, 0, size / 2, size / 2);
        painter.drawRect(size / 2, size / 2, size / 2, size / 2);
        painter.restore();
    }

    // Draw color square with rounded corners and subtle border
    painter.setPen(QPen(QColor(0, 0, 0, 60), 1));
    painter.setBrush(color);
    painter.drawRoundedRect(QRectF(0.5, 0.5, size - 1, size - 1), 2, 2);

    return pixmap;
}

}  // namespace

//////////////////////////////////////////////
JsonTreeModel::JsonTreeModel(QObject* parent)
    : QAbstractItemModel(parent),
      m_root_item(new JsonTreeItem("root", "", 'r', "", nullptr))
{
    m_root_shared             = std::shared_ptr<JsonTreeItem>(m_root_item);
    m_root_item->has_children = true;
    refreshDesign();
}

JsonTreeModel::~JsonTreeModel()
{
    qprintt << "~JsonTreeModel: Cleaning up";

    // With shared_ptr and auto-cleanup chain, threads will clean themselves up
    // We just need to clear the queue and fetching set
    m_fetch_queue.clear();
    m_fetching_items.clear();

    m_root_item = nullptr;
    m_root_shared.reset();

    qprintt << "~JsonTreeModel: Done (strategy refcount:"
            << (m_strategy ? m_strategy.use_count() : 0) << ")";
}

void JsonTreeModel::refreshDesign()
{
    m_isDarkMode = qApp->palette().color(QPalette::Window).lightness() < 128;
    qreal dpr    = qApp->devicePixelRatio();
    int iconSize = 18;

    QColor objColor
        = m_isDarkMode ? QColor(206, 147, 216) : QColor(156, 39, 176);
    m_objIcon = svgIcon(g_svg_object, objColor, iconSize, dpr);

    QColor arrColor
        = m_isDarkMode ? QColor(128, 222, 234) : QColor(0, 188, 212);
    m_arrIcon = svgIcon(g_svg_array, arrColor, iconSize, dpr);
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
    m_strategy  = strategy;
    m_file_mode = strategy ? strategy->type() : FileMode::Small;

    // Check for parse errors first
    const auto* metrics = strategy ? &strategy->metrics() : nullptr;
    if (metrics && !metrics->parseError.isEmpty()) {
        // Create error tree instead of normal tree
        beginResetModel();
        m_root_item = nullptr;
        m_root_shared.reset();
        m_root_shared = createErrorTree(metrics);
        m_root_item   = m_root_shared.get();
        endResetModel();

        // Still return success to prevent Seer from switching viewers
        emit loadFinished(true, elapsedMs);
        return;
    }

    if (!success) {
        qprintt << "[LOAD ASYNC] Load failed (no parse error details)";
        emit loadFinished(false, elapsedMs);
        return;
    }

    qprintt << "[LOAD ASYNC] Completed in" << elapsedMs << "ms";

    // Normal flow: update model with parsed tree
    beginResetModel();
    m_root_item = nullptr;
    m_root_shared.reset();
    m_root_shared = root;        // Keep shared_ptr alive
    m_root_item   = root.get();  // Raw pointer for quick access

    endResetModel();

    qprintt << "=== [LOAD END] Total time:" << elapsedMs << "ms ===";
    emit loadFinished(true, elapsedMs);
}

std::shared_ptr<JsonTreeItem> JsonTreeModel::createErrorTree(
    const JsonViewerStrategy::Metrics* metrics)
{
    // Create a dummy root (will be hidden by QTreeView)
    auto root = std::make_shared<JsonTreeItem>("root", "", 'r', "", nullptr);
    root->has_children    = true;
    root->children_loaded = true;

    // Add "⚠️ Parse Error" as the first visible node (no value to avoid
    // duplication with Context)
    auto* errorNode            = new JsonTreeItem("⚠️ Parse Error", "", 'E', "",
                                                  root.get());  // Empty value
    errorNode->has_children    = true;
    errorNode->children_loaded = true;
    root->children.append(errorNode);

    // Add error details as children of the error node
    auto* errorMsg = new JsonTreeItem("Error Message", "", 's',
                                      metrics->parseError, errorNode);
    errorNode->children.append(errorMsg);

    // Always show position (even if 0)
    auto* position = new JsonTreeItem(
        "Position", "", 'n', QString::number(metrics->errorOffset), errorNode);
    errorNode->children.append(position);

    if (!metrics->errorContext.isEmpty()) {
        auto* context = new JsonTreeItem("Context", "", 's',
                                         metrics->errorContext, errorNode);
        errorNode->children.append(context);
    }

    return root;
}

QVariant JsonTreeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return {};
    }

    auto item = static_cast<JsonTreeItem*>(index.internalPointer());

    // Dim array indices (column 0) to distinguish them from real object keys
    if (role == Qt::ForegroundRole && index.column() == CI_Key) {
        // Check if this item's parent is an array
        if (item->parent && item->parent->type == '[') {
            // Array index - use secondary text color (dimmed)
            bool isDark
                = qApp->palette().color(QPalette::Window).lightness() < 128;
            // Use 70% opacity for array indices
            QColor textColor = qApp->palette().color(QPalette::Text);
            textColor.setAlpha(180);  // ~70% opacity (180/255)
            return textColor;
        }
        return {};  // Use default text color for object keys
    }

    // Type-based coloring for values (column 1)
    if (role == Qt::ForegroundRole && index.column() == CI_Value) {
        // Use refined, accessible colors that work in both light and dark
        // themes Colors are chosen based on Seer's design principles: clean,
        // sharp, refined

        // Detect if we're in dark mode by checking the palette
        // For containers (objects/arrays), use dimmed text for the preview
        if (item->has_children) {
            QColor textColor = qApp->palette().color(QPalette::Text);
            textColor.setAlpha(180);  // ~70% opacity
            return textColor;
        }

        switch (item->type) {
        case 's':  // String - green
            return m_isDarkMode
                       ? QColor(129, 199, 132)  // Softer green for dark
                       : QColor(76, 175, 80);   // Material green for light
        case 'n':                               // Number - blue
            return m_isDarkMode
                       ? QColor(100, 181, 246)  // Lighter blue for dark
                       : QColor(33, 150, 243);  // Material blue for light
        case 'b':                               // Boolean - orange
            return m_isDarkMode
                       ? QColor(255, 183, 77)  // Softer orange for dark
                       : QColor(255, 152, 0);  // Warm orange for light
        case 'N':                              // null - gray
            return m_isDarkMode
                       ? QColor(176, 176, 176)   // Lighter gray for dark
                       : QColor(158, 158, 158);  // Neutral gray for light
        default:
            return {};
        }
    }

    // Node icons for keys (column 0) and color preview for values (column 1)
    if (role == Qt::DecorationRole) {
        if (index.column() == CI_Key) {
            if (item->type == '{')
                return m_objIcon;
            if (item->type == '[')
                return m_arrIcon;
        }
        else if (index.column() == CI_Value && item->type == 's'
                 && Config::ins().showColorPreview()) {
            // Color preview for string values that look like colors
            QColor color = parseColorValue(item->value);
            if (color.isValid()) {
                qreal dpr = qApp->devicePixelRatio();
                // Match font height for premium look
                int size = QFontMetrics(qApp->font()).height();
                // Ensure it's not too large, 14-16 is usually good
                size = qBound(12, size - 2, 18);
                return createColorPreview(color, dpr, size);
            }
        }
        return {};
    }

    if (role == Qt::ToolTipRole) {
        if (index.column() == CI_Key) {
            return getDotPath(index);
        }
        if (index.column() == CI_Value) {
            return item->value;
        }
    }

    if (role == Qt::DisplayRole) {
        if (index.column() == CI_Key) {
            return item->key;
        }
        if (index.column() == CI_Value) {
            // Container preview
            if (item->has_children) {
                if (item->type == '{') {
                    return item->child_count > 0 ? QString("Object (%1 fields)")
                                                       .arg(item->child_count)
                                                 : QString("Object");
                }
                else if (item->type == '[') {
                    return item->child_count > 0 ? QString("Array (%1 items)")
                                                       .arg(item->child_count)
                                                 : QString("Array");
                }
            }

            // Value is already processed by strategy (includes quotes and
            // truncation)
            return item->value;
        }
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

const JsonViewerStrategy::Metrics* JsonTreeModel::metrics() const
{
    return m_strategy ? &m_strategy->metrics() : nullptr;
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
    return !item->children_loaded && item->children.isEmpty();
}

void JsonTreeModel::fetchMore(const QModelIndex& parent)
{
    if (!canFetchMore(parent)) {
        // Special case: If this is the root and we're already loaded (e.g.
        // empty {}), we must still signal completion to hide the loading UI.
        if (getItem(parent) == m_root_item) {
            emit firstFetchCompleted(0);
        }
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

QString JsonTreeModel::getDotPath(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return QString();
    }

    JsonTreeItem* item = getItem(index);
    if (!item) {
        return QString();
    }

    // Build dot path by traversing up the tree
    QStringList parts;
    JsonTreeItem* current = item;

    while (current && current != m_root_item) {
        if (current->is_virtual_page) {
            // Skip virtual page nodes
            current = current->parent;
            continue;
        }

        QString key = current->key;

        // Check if parent is an array
        if (current->parent && current->parent != m_root_item
            && current->parent->type == '[') {
            // Array element: use [index] notation
            parts.prepend(QString("[%1]").arg(key));
        }
        else if (!key.isEmpty()) {
            // Object property: use dot notation
            // Escape key if it contains special characters
            if (key.contains('.') || key.contains('[') || key.contains(']')
                || key.contains(' ')) {
                parts.prepend(QString("[\"%1\"]").arg(key));
            }
            else {
                parts.prepend(key);
            }
        }

        current = current->parent;
    }

    // Join with dots, but handle [index] specially
    QString result;
    for (int i = 0; i < parts.size(); ++i) {
        if (i > 0 && !parts[i].startsWith('[')) {
            result += '.';
        }
        result += parts[i];
    }

    return result;
}

QString JsonTreeModel::getKeyValue(const QModelIndex& idx,
                                   bool* success,
                                   QString* errorMsg) const
{
    if (success)
        *success = false;

    if (!idx.isValid()) {
        if (errorMsg)
            *errorMsg = "Invalid index";
        return QString();
    }

    JsonTreeItem* item = getItem(idx);
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
        valueStr       = getSubtree(idx, &subtreeOk, &subtreeErr);
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

QString JsonTreeModel::getSubtree(const QModelIndex& idx,
                                  bool* success,
                                  QString* errorMsg) const
{
    if (success)
        *success = false;

    if (!idx.isValid()) {
        if (errorMsg)
            *errorMsg = "Invalid index";
        return QString();
    }

    JsonTreeItem* item = getItem(idx);
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

    if (success)
        *success = true;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &err);
    if (err.error != QJsonParseError::NoError) {
        return QString::fromUtf8(jsonData);
    }

    const auto& cfg = Config::ins();
    if (cfg.exportFormat() == "compact") {
        return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    }

    QByteArray result = doc.toJson(QJsonDocument::Indented);
    int indentSpaces  = cfg.exportIndentSpaces();
    if (indentSpaces == 4) {
        return QString::fromUtf8(result);
    }

    // Manual indentation replacement for custom space count
    QString str       = QString::fromUtf8(result);
    QStringList lines = str.split('\n');
    for (QString& line : lines) {
        int spaceCount = 0;
        while (spaceCount < line.length() && line[spaceCount] == ' ') {
            spaceCount++;
        }
        if (spaceCount > 0 && spaceCount % 4 == 0) {
            int levels = spaceCount / 4;
            line.replace(0, spaceCount, QString(levels * indentSpaces, ' '));
        }
    }
    return lines.join('\n');
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

    // Emit queue changed signal
    emit fetchQueueChanged(fetchQueueSize(), m_fetch_in_progress);

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
        emit fetchQueueChanged(0, false);
        return;
    }

    auto request       = m_fetch_queue.dequeue();
    JsonTreeItem* item = request.item;
    QModelIndex parent = request.parent;

    qprintt << "[FETCH ASYNC] Processing:" << item->key
            << "Queue size:" << m_fetch_queue.size();

    // Mark as in progress
    m_fetch_in_progress = true;

    // Emit queue changed signal
    emit fetchQueueChanged(fetchQueueSize(), true);

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
    auto* thread = new JTVThread;
    QPointer<FetchWorker> worker
        = new FetchWorker(m_strategy, fetch_pointer, fetch_offset, fetch_length,
                          item, parent, m_file_mode, page_start, page_end,
                          // Pass cached count to avoid cross-thread access
                          item->child_count);
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

    // Mark as loaded regardless of child count to prevent infinite fetch loops
    parent_item->children_loaded = true;

    // Insert actual children
    int count = children->size();
    if (count > 0) {
        beginInsertRows(valid_parent_index, 0, count - 1);

        // Set parent relationship
        for (auto* child : *children) {
            child->parent = parent_item;
        }
        parent_item->children = *children;

        // Cache the child count for future paging decisions
        if (parent_item->child_count == 0) {
            parent_item->child_count = count;
        }

        // Model takes ownership of these pointers
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