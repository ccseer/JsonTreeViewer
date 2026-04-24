#include "jsontreemodel.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QStringBuilder>

#include "jsonnode.h"
#include "logging.h"
#include "strategies/extremefilestrategy.h"
#include "strategies/jsonstrategy.h"
#include "strategies/largefilestrategy.h"
#include "strategies/mediumfilestrategy.h"
#include "strategies/smallfilestrategy.h"

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

QString JsonTreeModel::toEscaped(const QString& key)
{
    QString escaped;
    escaped.reserve(key.size() * 2);

    for (QChar ch : key) {
        if (ch == '~') {
            escaped += "~0";
        }
        else if (ch == '/') {
            escaped += "~1";
        }
        else {
            escaped += ch;
        }
    }
    return escaped;
}

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
    delete m_root_item;
}

bool JsonTreeModel::load(const QString& path)
{
    // Get file size to select strategy
    QFileInfo fileInfo(path);
    qint64 fileSize = fileInfo.size();

    qprintt << "Loading file:" << path << "Size:" << fileSize << "bytes";

    // Select strategy based on file size
    if (fileSize < StrategyThresholds::SMALL_FILE_MAX) {
        m_strategy  = std::make_unique<SmallFileStrategy>();
        m_file_mode = FileMode::Small;
        qprintt << "Using SmallFileStrategy";
    }
    else if (fileSize < StrategyThresholds::MEDIUM_FILE_MAX) {
        m_strategy  = std::make_unique<MediumFileStrategy>();
        m_file_mode = FileMode::Medium;
        qprintt << "Using MediumFileStrategy";
    }
    else if (fileSize < StrategyThresholds::LARGE_FILE_MAX) {
        m_strategy  = std::make_unique<LargeFileStrategy>();
        m_file_mode = FileMode::Large;
        qprintt << "Using LargeFileStrategy";
    }
    else {
        m_strategy  = std::make_unique<ExtremeFileStrategy>();
        m_file_mode = FileMode::Extreme;
        qprintt << "Using ExtremeFileStrategy";
    }

    // Load file using selected strategy
    if (!m_strategy->load(path)) {
        qprintt << "Strategy load failed";
        return false;
    }

    // Set root item metadata
    m_root_item->byte_offset = 0;
    m_root_item->byte_length = m_strategy->dataSize();

    // Extract first level children
    QVector<JsonTreeItem*> items;
    try {
        items = extractChildren(m_root_item);
        if (items.isEmpty()) {
            qprintt << "extractChildren: empty";
            return false;
        }
    }
    catch (...) {
        qprintt << "exception:" << Q_FUNC_INFO;
        return false;
    }

    beginInsertRows({}, 0, items.count() - 1);
    m_root_item->children.append(items);
    m_root_item->children_loaded = true;
    endInsertRows();
    m_root_item->has_children = !m_root_item->children.isEmpty();
    return true;
}

void JsonTreeModel::loadEverything()
{
    QElapsedTimer et;
    et.start();
    beginResetModel();
    std::function<void(JsonTreeItem*)> recursiveLoad
        = [this, &recursiveLoad](JsonTreeItem* item) {
              if (item->has_children && item->children.isEmpty()) {
                  item->children = extractChildren(item);
              }
              for (JsonTreeItem* child : item->children) {
                  recursiveLoad(child);
              }
          };
    recursiveLoad(m_root_item);
    endResetModel();
    qprintt << "loadEverything" << et.elapsed() << "ms";
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
    return item && item->has_children && item->children.isEmpty();
}

void JsonTreeModel::fetchMore(const QModelIndex& parent)
{
    if (!canFetchMore(parent)) {
        return;
    }
    JsonTreeItem* item             = getItem(parent);
    const auto old_t               = item->children.size();
    QList<JsonTreeItem*> new_items = extractChildren(item);
    const auto insert_t            = new_items.size();
    if (insert_t > 0) {
        beginInsertRows(parent, old_t, old_t + insert_t - 1);
        item->children.append(new_items);
        item->children_loaded = true;
        endInsertRows();
    }
}

JsonTreeItem* JsonTreeModel::getItem(const QModelIndex& index) const
{
    if (index.isValid()) {
        return static_cast<JsonTreeItem*>(index.internalPointer());
    }
    return m_root_item;
}

QVector<JsonTreeItem*> JsonTreeModel::extractChildren(JsonTreeItem* parent_item)
{
    if (!parent_item || !parent_item->has_children) {
        return {};
    }
    if (!parent_item->children.isEmpty()) {
        return {};
    }

    // Use strategy to extract children
    if (m_strategy) {
        return m_strategy->extractChildren(parent_item);
    }

    return {};
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
