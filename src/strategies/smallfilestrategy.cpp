#include "smallfilestrategy.h"

#include <QDebug>

#include "../jsonnode.h"
#include "../jsontreemodel.h"
#include "../logging.h"
#include "jsonstrategy.h"

#define qprintt qprint << "[SmallFileStrategy]"

namespace {
constexpr auto g_type_obj = "···";
constexpr auto g_type_arr = "···";

QPair<char, QString> domTypeAndPreview(simdjson::dom::element value)
{
    if (value.is_object())
        return {'{', QString("{%1}").arg(g_type_obj)};
    if (value.is_array())
        return {'[', QString("[%1]").arg(g_type_arr)};
    if (value.is_string()) {
        std::string_view sv = value.get_string();
        return {'s', QString::fromUtf8(sv.data(), sv.size())};
    }
    if (value.is_number()) {
        if (value.is_int64())
            return {'n', QString::number(value.get_int64().value())};
        if (value.is_uint64())
            return {'n', QString::number(value.get_uint64().value())};
        return {'n', QString::number(value.get_double().value())};
    }
    if (value.is_bool())
        return {'b', value.get_bool().value() ? "true" : "false"};
    if (value.is_null())
        return {'N', "null"};
    return {'?', {}};
}
}  // namespace

SmallFileStrategy::SmallFileStrategy()  = default;
SmallFileStrategy::~SmallFileStrategy() = default;

bool SmallFileStrategy::load(const QString& path)
{
    qprintt << "Loading small file:" << path;
    auto result = simdjson::padded_string::load(path.toUtf8().data());
    if (result.error()) {
        qprintt << "Failed to load:" << simdjson::error_message(result.error());
        return false;
    }
    m_json_data = std::move(result.value());

    auto parse_result = m_parser.parse(m_json_data);
    if (parse_result.error()) {
        qprintt << "Parse error:"
                << simdjson::error_message(parse_result.error());
        return false;
    }
    m_root = parse_result.value();
    return true;
}

QVector<JsonTreeItem*> SmallFileStrategy::extractChildren(
    JsonTreeItem* parent_item)
{
    if (!parent_item || !parent_item->has_children)
        return {};
    if (!parent_item->children.isEmpty())
        return {};

    QVector<JsonTreeItem*> items_vec;
    try {
        simdjson::dom::element element
            = parent_item->pointer.isEmpty()
                  ? m_root
                  : m_root.at_pointer(parent_item->pointer.toStdString())
                        .value();

        const char* base_ptr
            = reinterpret_cast<const char*>(m_json_data.data());

        if (element.is_object()) {
            for (auto field : element.get_object()) {
                std::string_view key = field.key;
                QString key_str = QString::fromUtf8(key.data(), key.size());
                auto [vt, vp]   = domTypeAndPreview(field.value);

                auto* item
                    = new JsonTreeItem(key_str,
                                       parent_item->pointer + "/"
                                           + JsonTreeModel::toEscaped(key_str),
                                       vt, vp, parent_item);
                item->has_children = hasChildren(vt);

                // Calculate byte offset from DOM element pointer
                // Note: This is approximate for DOM, but good enough for small
                // files
                const char* value_ptr
                    = reinterpret_cast<const char*>(&field.value);
                if (value_ptr >= base_ptr
                    && value_ptr < base_ptr + m_json_data.size()) {
                    item->byte_offset = value_ptr - base_ptr;
                    item->byte_length
                        = 0;  // DOM doesn't provide exact length easily
                }

                items_vec.append(item);
            }
        }
        else if (element.is_array()) {
            int idx = 0;
            for (auto value : element.get_array()) {
                auto [vt, vp]   = domTypeAndPreview(value);
                QString idx_str = QString::number(idx);

                auto* item = new JsonTreeItem(
                    idx_str, parent_item->pointer + "/" + idx_str, vt, vp,
                    parent_item);
                item->has_children = hasChildren(vt);

                // Calculate byte offset
                const char* value_ptr = reinterpret_cast<const char*>(&value);
                if (value_ptr >= base_ptr
                    && value_ptr < base_ptr + m_json_data.size()) {
                    item->byte_offset = value_ptr - base_ptr;
                    item->byte_length = 0;
                }

                items_vec.append(item);
                ++idx;
            }
        }
    }
    catch (simdjson::simdjson_error& e) {
        qprintt << "Error extracting children:" << e.what();
    }
    return items_vec;
}

const char* SmallFileStrategy::dataPtr() const
{
    return reinterpret_cast<const char*>(m_json_data.data());
}

size_t SmallFileStrategy::dataSize() const
{
    return m_json_data.size();
}

const JsonViewerStrategy::Metrics& SmallFileStrategy::metrics() const
{
    return m_metrics;
}
