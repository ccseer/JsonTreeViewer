#include "jsonstrategy.h"

#include <QDebug>

#include "../jsonnode.h"
#include "../logging.h"

#define qprintt qprint << "[JsonStrategy]"

namespace {
constexpr auto g_type_obj = "\xc2\xb7\xc2\xb7\xc2\xb7";
constexpr auto g_type_arr = "\xc2\xb7\xc2\xb7\xc2\xb7";

QPair<char, QString> typeAndPreviewFromRaw(const char* data_ptr,
                                           size_t offset,
                                           size_t length)
{
    if (length == 0)
        return {'?', {}};
    char first = data_ptr[offset];
    if (first == '{')
        return {'{', QString("{%1}").arg(g_type_obj)};
    if (first == '[')
        return {'[', QString("[%1]").arg(g_type_arr)};
    if (first == '"') {
        size_t content_len = length > 2 ? length - 2 : 0;
        if (content_len > 100)
            return {'s', QString::fromUtf8(data_ptr + offset + 1, 100) + "..."};
        return {'s', QString::fromUtf8(data_ptr + offset + 1,
                                       static_cast<int>(content_len))};
    }
    if (first == 't' || first == 'f')
        return {'b', QString::fromUtf8(data_ptr + offset, length)};
    if (first == 'n')
        return {'N', "null"};
    return {'n', QString::fromUtf8(data_ptr + offset, length)};
}

bool hasChildren(char type)
{
    return type == '{' || type == '[';
}

// Shared iteration for ondemand::value (at_pointer path).
// Uses type() to select get_object / get_array without polluting ondemand
// state.
QVector<JsonTreeItem*> iterateValue(simdjson::ondemand::value& container,
                                    JsonTreeItem* parent_item,
                                    const char* base_ptr,
                                    const char* parse_ptr,
                                    size_t base_off,
                                    bool range_mode,
                                    int start,
                                    int end)
{
    QVector<JsonTreeItem*> items_vec;

    auto type_res = container.type();
    if (type_res.error()) {
        qprintt << "Failed to get container type:"
                << simdjson::error_message(type_res.error());
        return items_vec;
    }
    const auto node_type = type_res.value_unsafe();

    if (node_type == simdjson::ondemand::json_type::object) {
        auto obj_res = container.get_object();
        if (obj_res.error())
            return items_vec;
        auto obj = obj_res.value_unsafe();
        try {
            int idx = 0;
            for (auto field : obj) {
                if (range_mode && idx < start) {
                    ++idx;
                    continue;
                }
                if (range_mode && idx > end)
                    break;

                auto fv        = field.value().value();
                auto raw       = fv.raw_json_token();
                size_t abs_off = (raw.data() - parse_ptr) + base_off;
                size_t len     = raw.size();

                std::string_view key = field.unescaped_key();
                QString key_str = QString::fromUtf8(key.data(), key.size());
                QString esc_key = key_str;
                esc_key.replace("~", "~0").replace("/", "~1");
                auto [vt, vp] = typeAndPreviewFromRaw(base_ptr, abs_off, len);

                auto* item = new JsonTreeItem(
                    key_str, parent_item->pointer + "/" + esc_key, vt, vp,
                    parent_item);
                item->has_children = hasChildren(vt);
                item->byte_offset  = abs_off;
                item->byte_length  = len;
                items_vec.append(item);
                ++idx;
            }
            parent_item->child_count = static_cast<quint32>(idx);
        }
        catch (...) {
            qprintt << "Exception while parsing object fields";
        }
    }
    else if (node_type == simdjson::ondemand::json_type::array) {
        auto arr_res = container.get_array();
        if (arr_res.error())
            return items_vec;
        auto arr = arr_res.value_unsafe();
        try {
            int idx = 0;
            for (auto element : arr) {
                if (range_mode && idx < start) {
                    ++idx;
                    continue;
                }
                if (range_mode && idx > end)
                    break;

                auto ev        = element.value();
                auto raw       = ev.raw_json_token();
                size_t abs_off = (raw.data() - parse_ptr) + base_off;
                size_t len     = raw.size();

                auto [vt, vp]   = typeAndPreviewFromRaw(base_ptr, abs_off, len);
                QString idx_str = QString::number(idx);
                auto* item      = new JsonTreeItem(
                    idx_str, parent_item->pointer + "/" + idx_str, vt, vp,
                    parent_item);
                item->has_children = hasChildren(vt);
                item->byte_offset  = abs_off;
                item->byte_length  = len;
                items_vec.append(item);
                ++idx;
            }
            parent_item->child_count = static_cast<quint32>(idx);
        }
        catch (...) {
            qprintt << "Exception while parsing array elements";
        }
    }

    return items_vec;
}

}  // namespace

quint32 JsonViewerStrategy::countLocalBufferChildren(JsonTreeItem* parent_item,
                                                     const char* base_ptr,
                                                     size_t base_size)
{
    if (!parent_item || !parent_item->has_children)
        return 0;

    const size_t padding = simdjson::SIMDJSON_PADDING;

    simdjson::ondemand::parser parser;
    auto iter_result = parser.iterate(base_ptr, base_size, base_size + padding);
    if (iter_result.error())
        return 0;
    auto& doc = iter_result.value_unsafe();

    if (parent_item->pointer.isEmpty()) {
        auto obj_res = doc.get_object();
        if (!obj_res.error()) {
            auto count_res = obj_res.value_unsafe().count_fields();
            if (!count_res.error())
                return static_cast<quint32>(count_res.value_unsafe());
        }
        simdjson::ondemand::parser parser2;
        auto iter2 = parser2.iterate(base_ptr, base_size, base_size + padding);
        if (!iter2.error()) {
            auto arr_res = iter2.value_unsafe().get_array();
            if (!arr_res.error()) {
                auto count_res = arr_res.value_unsafe().count_elements();
                if (!count_res.error())
                    return static_cast<quint32>(count_res.value_unsafe());
            }
        }
        return 0;
    }
    else {
        auto ptr_result = doc.at_pointer(parent_item->pointer.toStdString());
        if (ptr_result.error())
            return 0;
        auto target   = ptr_result.value_unsafe();
        auto type_res = target.type();
        if (type_res.error())
            return 0;
        if (type_res.value_unsafe() == simdjson::ondemand::json_type::object) {
            auto obj_res = target.get_object();
            if (!obj_res.error()) {
                auto count_res = obj_res.value_unsafe().count_fields();
                if (!count_res.error())
                    return static_cast<quint32>(count_res.value_unsafe());
            }
        }
        else if (type_res.value_unsafe()
                 == simdjson::ondemand::json_type::array) {
            auto arr_res = target.get_array();
            if (!arr_res.error()) {
                auto count_res = arr_res.value_unsafe().count_elements();
                if (!count_res.error())
                    return static_cast<quint32>(count_res.value_unsafe());
            }
        }
        return 0;
    }
}

// O(slice) parsing: when byte_offset + byte_length are recorded we parse only
// that slice, avoiding a full O(N) document scan for every expand.
//
// range_mode (start >= 0): items outside [start, end] are skipped without
// allocating JsonTreeItem objects, making virtual-page expansion O(page_size).
QVector<JsonTreeItem*> JsonViewerStrategy::parseLocalBuffer(
    JsonTreeItem* parent_item,
    const char* base_ptr,
    size_t base_size,
    int start,
    int end)
{
    if (!parent_item || !parent_item->has_children)
        return {};

    const bool range_mode = (start >= 0 && end >= start);

    // In range_mode the caller already holds virtual-page children on the
    // actual_parent node - skip the isEmpty guard so we can re-parse.
    if (!range_mode && !parent_item->children.isEmpty())
        return {};

    const size_t padding = simdjson::SIMDJSON_PADDING;

    QVector<JsonTreeItem*> items_vec;

    simdjson::ondemand::parser parser;
    auto iter_result = parser.iterate(base_ptr, base_size, base_size + padding);
    if (iter_result.error()) {
        qprintt << "Failed to iterate:"
                << simdjson::error_message(iter_result.error());
        return {};
    }
    auto& doc = iter_result.value_unsafe();

    if (parent_item->pointer.isEmpty()) {
        // Document root: try object then array directly.
        // Calling type() on ondemand::document requires get_value() in some
        // versions, so we try get_object first, then get_array on a fresh
        // document parse.
        {
            auto obj_res = doc.get_object();
            if (!obj_res.error()) {
                auto obj = obj_res.value_unsafe();
                try {
                    int idx = 0;
                    for (auto field : obj) {
                        if (range_mode && idx < start) {
                            ++idx;
                            continue;
                        }
                        if (range_mode && idx > end)
                            break;
                        auto fv              = field.value().value();
                        auto raw             = fv.raw_json_token();
                        size_t abs_off       = (raw.data() - base_ptr);
                        size_t len           = raw.size();
                        std::string_view key = field.unescaped_key();
                        QString key_str
                            = QString::fromUtf8(key.data(), key.size());
                        QString esc_key = key_str;
                        esc_key.replace("~", "~0").replace("/", "~1");
                        auto [vt, vp]
                            = typeAndPreviewFromRaw(base_ptr, abs_off, len);
                        auto* item = new JsonTreeItem(
                            key_str, parent_item->pointer + "/" + esc_key, vt,
                            vp, parent_item);
                        item->has_children = hasChildren(vt);
                        item->byte_offset  = abs_off;
                        item->byte_length  = len;
                        items_vec.append(item);
                        ++idx;
                    }
                    parent_item->child_count = static_cast<quint32>(idx);
                }
                catch (...) {
                    qprintt << "Exception while parsing object fields";
                }
                return items_vec;
            }
        }
        // Not an object - re-parse as array (fresh iterator required)
        {
            simdjson::ondemand::parser parser2;
            auto iter2
                = parser2.iterate(base_ptr, base_size, base_size + padding);
            if (!iter2.error()) {
                auto& doc2    = iter2.value_unsafe();
                auto arr_res2 = doc2.get_array();
                if (!arr_res2.error()) {
                    auto arr = arr_res2.value_unsafe();
                    try {
                        int idx = 0;
                        for (auto element : arr) {
                            if (range_mode && idx < start) {
                                ++idx;
                                continue;
                            }
                            if (range_mode && idx > end)
                                break;
                            auto ev        = element.value();
                            auto raw       = ev.raw_json_token();
                            size_t abs_off = (raw.data() - base_ptr);
                            size_t len     = raw.size();
                            auto [vt, vp]
                                = typeAndPreviewFromRaw(base_ptr, abs_off, len);
                            QString idx_str = QString::number(idx);
                            auto* item      = new JsonTreeItem(
                                idx_str, parent_item->pointer + "/" + idx_str,
                                vt, vp, parent_item);
                            item->has_children = hasChildren(vt);
                            item->byte_offset  = abs_off;
                            item->byte_length  = len;
                            items_vec.append(item);
                            ++idx;
                        }
                        parent_item->child_count = static_cast<quint32>(idx);
                    }
                    catch (...) {
                        qprintt << "Exception while parsing array elements";
                    }
                    return items_vec;
                }
            }
        }
        qprintt << "Failed to parse document root as object or array";
        return items_vec;
    }
    else {
        auto ptr_result = doc.at_pointer(parent_item->pointer.toStdString());
        if (ptr_result.error()) {
            qprintt << "Failed to navigate to pointer:" << parent_item->pointer
                    << "Error:" << simdjson::error_message(ptr_result.error());
            return {};
        }
        auto target = ptr_result.value_unsafe();
        return iterateValue(target, parent_item, base_ptr, base_ptr, 0,
                            range_mode, start, end);
    }
}
