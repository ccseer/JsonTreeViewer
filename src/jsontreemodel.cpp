#include "jsontreemodel.h"

#include <QElapsedTimer>
#include <QStringBuilder>

#include "jsonnode.h"

using namespace simdjson;

#define qprintt qDebug() << "[JsonTreeViewer]"

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
    if (auto e = padded_string::load(path.toUtf8().data()).get(m_json_data)) {
        qprintt << "Failed to load file with alignment" << e;
        return false;
    }

    QVector<JsonTreeItem*> items;
    try {
        m_doc = m_parser.iterate(m_json_data);
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

QPair<char, QString> JsonTreeModel::getTypeAndPreview(
    ondemand::value value) const
{
    using Type           = ondemand::json_type;
    const auto type_char = static_cast<char>(value.type().value());
    switch (value.type()) {
    case Type::object: {
        return {type_char, QString("{%1}").arg(g_type_obj)};
    }
    case Type::array: {
        return {type_char, QString("[%1]").arg(g_type_arr)};
    }
    case Type::string: {
        std::string_view str;
        value.get_string().get(str);
        return {type_char, QString::fromUtf8(str.data(), str.size())};
    }
    case Type::number: {
        ondemand::number_type num_type;
        if (value.get_number_type().get(num_type) != SUCCESS) {
            return {type_char, "Invalid Number"};
        }
        QString num_str;
        switch (num_type) {
        case ondemand::number_type::signed_integer: {
            int64_t num;
            value.get_int64().get(num);
            num_str = QString::number(num);
            break;
        }
        case ondemand::number_type::unsigned_integer: {
            uint64_t num;
            value.get_uint64().get(num);
            num_str = QString::number(num);
            break;
        }
        case ondemand::number_type::big_integer:
        case ondemand::number_type::floating_point_number: {
            std::string_view raw = value.raw_json_token();
            num_str = QString::fromUtf8(raw.data(), raw.size()).trimmed();
            break;
        }
        }
        return {type_char, num_str};
    }
    case Type::boolean: {
        bool b = false;
        value.get_bool().get(b);
        return {type_char, b ? "true" : "false"};
    }
    case Type::null: {
        return {type_char, {}};
    }
    }
    return {type_char, {}};
}

bool JsonTreeModel::hasChildNode(ondemand::value value) const
{
    using Type = ondemand::json_type;

    if (value.type() == Type::object) {
        ondemand::object obj;
        return !value.get_object().get(obj) && obj.begin() != obj.end();
    }
    else if (value.type() == Type::array) {
        ondemand::array arr;
        return !value.get_array().get(arr) && arr.begin() != arr.end();
    }

    return false;
}

QVector<JsonTreeItem*> JsonTreeModel::extractChildren(JsonTreeItem* parent_item)
{
    if (!parent_item || !parent_item->has_children) {
        return {};
    }
    if (!parent_item->children.isEmpty()) {
        return {};
    }

    ondemand::value value;
    if (parent_item->pointer.isEmpty()) {
        value = m_doc;
    }
    else {
        auto result = m_doc.at_pointer(parent_item->pointer.toStdString());
        if (result.error()) {
            qprintt << "at_pointer error:" << parent_item->pointer;
            return {};
        }
        value = result.value();
    }
    QVector<JsonTreeItem*> items_vec;
    ondemand::json_type type = value.type();
    if (type == ondemand::json_type::object) {
        ondemand::object obj = value.get_object();
        parent_item->children.reserve(obj.count_fields());
        for (auto field : obj) {
            auto field_val       = field.value().value();
            auto [val_type, val] = getTypeAndPreview(field_val);
            std::string_view key = field.unescaped_key();
            auto field_name      = QString::fromUtf8(key.data(), key.size());
            auto item            = new JsonTreeItem(
                field_name, parent_item->pointer % "/" % toEscaped(field_name),
                val_type, val, parent_item);
            item->has_children = hasChildNode(field_val);
            items_vec.append(item);
        }
    }
    else if (type == ondemand::json_type::array) {
        ondemand::array arr = value.get_array();
        parent_item->children.reserve(arr.count_elements());
        int index = 0;
        for (auto element : arr) {
            auto ele_val         = element.value();
            auto [val_type, val] = getTypeAndPreview(ele_val);
            auto item            = new JsonTreeItem(
                QString::number(index),
                parent_item->pointer % "/" % QString::number(index), val_type,
                val, parent_item);
            item->has_children = hasChildNode(ele_val);
            items_vec.append(item);
            ++index;
        }
    }
    return items_vec;
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
