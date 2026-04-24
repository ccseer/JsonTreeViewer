#include "jsonstrategy.h"

#include <QDebug>

#include "../jsonnode.h"
#include "../logging.h"

#define qprintt qprint << "[JsonStrategy]"

namespace {
constexpr auto g_type_obj = "···";
constexpr auto g_type_arr = "···";
}  // namespace

QPair<char, QString> JsonViewerStrategy::typeAndPreviewFromRaw(
    const char* data_ptr, size_t offset, size_t length)
{
    if (length == 0)
        return {'?', {}};
    char first = data_ptr[offset];
    if (first == '{')
        return {'{', QString("{%1}").arg(g_type_obj)};
    if (first == '[')
        return {'[', QString("[%1]").arg(g_type_arr)};
    if (first == '"') {
        if (length > 102)
            return {'s', QString::fromUtf8(data_ptr + offset + 1, 100) + "..."};
        return {'s', QString::fromUtf8(data_ptr + offset + 1,
                                       static_cast<int>(length) - 2)};
    }
    if (first == 't' || first == 'f')
        return {'b', QString::fromUtf8(data_ptr + offset, length)};
    if (first == 'n')
        return {'N', "null"};
    return {'n', QString::fromUtf8(data_ptr + offset, length)};
}

QVector<JsonTreeItem*> JsonViewerStrategy::parseLocalBuffer(
    JsonTreeItem* parent_item, const char* base_ptr, size_t base_size)
{
    if (!parent_item || !parent_item->has_children)
        return {};
    if (!parent_item->children.isEmpty())
        return {};

    // Always parse the whole document and use JSON Pointer to navigate
    size_t parse_offset = 0;
    size_t parse_length = base_size;

    const size_t padding = simdjson::SIMDJSON_PADDING;
    const char* parse_ptr = base_ptr;

    QVector<JsonTreeItem*> items_vec;

    // Create parser
    simdjson::ondemand::parser parser;

    auto iterate_result = parser.iterate(parse_ptr, parse_length, parse_length + padding);
    if (iterate_result.error()) {
        qprintt << "Failed to iterate:" << simdjson::error_message(iterate_result.error());
        return items_vec;
    }

    // Get document reference (cannot copy ondemand::document)
    auto& doc = iterate_result.value_unsafe();

    // Navigate to the parent node using JSON Pointer if needed
    simdjson::ondemand::value target_value;
    if (parent_item->pointer.isEmpty()) {
        // Root node - use document directly
        target_value = doc;
    } else {
        // Navigate using JSON Pointer
        auto pointer_result = doc.at_pointer(parent_item->pointer.toStdString());
        if (pointer_result.error()) {
            qprintt << "Failed to navigate to pointer:" << parent_item->pointer
                    << "Error:" << simdjson::error_message(pointer_result.error());
            return items_vec;
        }
        target_value = pointer_result.value_unsafe();
    }

    // Try to get as object first
    auto obj_result = target_value.get_object();
    if (!obj_result.error()) {
        auto obj = obj_result.value_unsafe();

        try {
            for (auto field : obj) {
                auto fv = field.value().value();
                auto raw = fv.raw_json_token();

                // Calculate offset relative to base_ptr
                size_t abs_off = raw.data() - base_ptr;
                size_t len = raw.size();

                std::string_view key = field.unescaped_key();
                QString key_str = QString::fromUtf8(key.data(), key.size());

                auto [vt, vp] = typeAndPreviewFromRaw(base_ptr, abs_off, len);

                // Build JSON Pointer (need to escape special characters)
                QString escaped_key = key_str;
                escaped_key.replace("~", "~0").replace("/", "~1");

                auto* item = new JsonTreeItem(
                    key_str, parent_item->pointer + "/" + escaped_key, vt, vp,
                    parent_item);
                item->has_children = hasChildren(vt);
                item->byte_offset = abs_off;
                item->byte_length = len;
                items_vec.append(item);
            }
            parent_item->child_count = items_vec.size();
        } catch (...) {
            qprintt << "Exception while parsing object fields";
        }
        return items_vec;
    }

    // Try to get as array
    auto arr_result = target_value.get_array();
    if (!arr_result.error()) {
        auto arr = arr_result.value_unsafe();

        try {
            const int MAX_INITIAL_ELEMENTS = 1000;

            int idx = 0;
            for (auto element : arr) {
                if (idx >= MAX_INITIAL_ELEMENTS) {
                    qprintt << "Limiting initial array load to" << MAX_INITIAL_ELEMENTS << "elements";
                    break;
                }

                auto ev = element.value();
                auto raw = ev.raw_json_token();

                size_t abs_off = raw.data() - base_ptr;
                size_t len = raw.size();

                auto [vt, vp] = typeAndPreviewFromRaw(base_ptr, abs_off, len);
                QString idx_str = QString::number(idx);

                auto* item = new JsonTreeItem(
                    idx_str, parent_item->pointer + "/" + idx_str, vt, vp,
                    parent_item);
                item->has_children = hasChildren(vt);
                item->byte_offset = abs_off;
                item->byte_length = len;
                items_vec.append(item);
                ++idx;
            }
            parent_item->child_count = items_vec.size();
        } catch (...) {
            qprintt << "Exception while parsing array elements";
        }
        return items_vec;
    }

    qprintt << "Failed to parse as object or array";
    return items_vec;
}
