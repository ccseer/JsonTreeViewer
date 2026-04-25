#include "jsonstrategy.h"

#include <QDebug>
#include <QThread>
#include <limits>

#include "../jsonnode.h"
#include "../logging.h"
#include "extremefilestrategy.h"
#include "largefilestrategy.h"
#include "mediumfilestrategy.h"
#include "smallfilestrategy.h"

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
// state. Now accepts parent_pointer instead of parent_item.
QVector<JsonTreeItem*> iterateValue(simdjson::ondemand::value& container,
                                    const QString& parent_pointer,
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
                // Check for interruption periodically
                if (idx % 100 == 0
                    && QThread::currentThread()->isInterruptionRequested()) {
                    qprintt << "Interrupted during object iteration at index"
                            << idx;
                    qDeleteAll(items_vec);
                    return {};
                }

                if (range_mode && idx < start) {
                    ++idx;
                    continue;
                }
                if (range_mode && idx > end)
                    break;

                auto fv        = field.value().value();
                auto raw_tok   = fv.raw_json_token();
                size_t abs_off = (raw_tok.data() - parse_ptr) + base_off;
                char first_ch  = raw_tok.empty() ? '?' : raw_tok[0];
                size_t len;
                if (first_ch == '{' || first_ch == '[') {
                    auto raw_res = fv.raw_json();
                    len          = raw_res.error() ? raw_tok.size()
                                                   : raw_res.value_unsafe().size();
                }
                else {
                    len = raw_tok.size();
                }
                auto [vt, vp] = typeAndPreviewFromRaw(base_ptr, abs_off, len);

                std::string_view key = field.unescaped_key();
                QString key_str = QString::fromUtf8(key.data(), key.size());
                QString esc_key = key_str;
                esc_key.replace("~", "~0").replace("/", "~1");

                auto* item = new JsonTreeItem(
                    key_str, parent_pointer + "/" + esc_key, vt, vp,
                    nullptr);  // parent will be set by caller
                item->has_children = hasChildren(vt);
                item->byte_offset  = abs_off;
                item->byte_length  = len;
                items_vec.append(item);
                ++idx;
            }
        }
        catch (...) {
            qprintt << "Exception while parsing object fields";
            qDeleteAll(items_vec);
            return {};
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
                // Check for interruption periodically
                if (idx % 100 == 0
                    && QThread::currentThread()->isInterruptionRequested()) {
                    qprintt << "Interrupted during array iteration at index"
                            << idx;
                    qDeleteAll(items_vec);
                    return {};
                }

                if (range_mode && idx < start) {
                    ++idx;
                    continue;
                }
                if (range_mode && idx > end)
                    break;

                auto ev        = element.value();
                auto raw_tok   = ev.raw_json_token();
                size_t abs_off = (raw_tok.data() - parse_ptr) + base_off;
                char first_ch  = raw_tok.empty() ? '?' : raw_tok[0];
                size_t len;
                if (first_ch == '{' || first_ch == '[') {
                    auto raw_res = ev.raw_json();
                    len          = raw_res.error() ? raw_tok.size()
                                                   : raw_res.value_unsafe().size();
                }
                else {
                    len = raw_tok.size();
                }

                auto [vt, vp]   = typeAndPreviewFromRaw(base_ptr, abs_off, len);
                QString idx_str = QString::number(idx);
                auto* item      = new JsonTreeItem(
                    idx_str, parent_pointer + "/" + idx_str, vt, vp,
                    nullptr);  // parent will be set by caller
                item->has_children = hasChildren(vt);
                item->byte_offset  = abs_off;
                item->byte_length  = len;
                items_vec.append(item);
                ++idx;
            }
        }
        catch (...) {
            qprintt << "Exception while parsing array elements";
            qDeleteAll(items_vec);
            return {};
        }
    }

    return items_vec;
}

}  // namespace

std::shared_ptr<JsonViewerStrategy> JsonViewerStrategy::createStrategy(
    qint64 fileSize)
{
    struct StrategyRule {
        qint64 maxSize;
        std::shared_ptr<JsonViewerStrategy> (*factory)();
        const char* name;
    };
    constexpr static StrategyRule rules[]
        = {{StrategyThresholds::SMALL_FILE_MAX,
            []() -> std::shared_ptr<JsonViewerStrategy> {
                return std::make_shared<SmallFileStrategy>();
            },
            "SmallFileStrategy"},
           {StrategyThresholds::MEDIUM_FILE_MAX,
            []() -> std::shared_ptr<JsonViewerStrategy> {
                return std::make_shared<MediumFileStrategy>();
            },
            "MediumFileStrategy"},
           {StrategyThresholds::LARGE_FILE_MAX,
            []() -> std::shared_ptr<JsonViewerStrategy> {
                return std::make_shared<LargeFileStrategy>();
            },
            "LargeFileStrategy"},
           {(std::numeric_limits<qint64>::max)(),
            []() -> std::shared_ptr<JsonViewerStrategy> {
                return std::make_shared<ExtremeFileStrategy>();
            },
            "ExtremeFileStrategy"}};

    for (const auto& rule : rules) {
        if (fileSize < rule.maxSize) {
            qprintt << "Using" << rule.name << fileSize;
            return rule.factory();
        }
    }

    // Fallback (should not reach here)
    return std::make_shared<ExtremeFileStrategy>();
}

quint32 JsonViewerStrategy::countLocalBufferChildren(const char* base_ptr,
                                                     size_t base_size)
{
    const size_t padding = simdjson::SIMDJSON_PADDING;

    simdjson::ondemand::parser parser;
    auto iter_result = parser.iterate(base_ptr, base_size, base_size + padding);
    if (iter_result.error())
        return 0;
    auto& doc = iter_result.value_unsafe();

    // Try as object first
    auto obj_res = doc.get_object();
    if (!obj_res.error()) {
        auto count_res = obj_res.value_unsafe().count_fields();
        if (!count_res.error())
            return static_cast<quint32>(count_res.value_unsafe());
    }

    // Try as array (need fresh parser)
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

// O(slice) parsing: when byte_offset + byte_length are recorded we parse only
// that slice, avoiding a full O(N) document scan for every expand.
//
// range_mode (start >= 0): items outside [start, end] are skipped without
// allocating JsonTreeItem objects, making virtual-page expansion O(page_size).
QVector<JsonTreeItem*> JsonViewerStrategy::parseLocalBuffer(
    const QString& parent_pointer,
    const char* base_ptr,
    size_t base_size,
    int start,
    int end)
{
    const bool range_mode    = (start >= 0 && end >= start);
    constexpr size_t padding = simdjson::SIMDJSON_PADDING;

    QVector<JsonTreeItem*> items_vec;

    // Check for interruption before starting
    if (QThread::currentThread()->isInterruptionRequested()) {
        qprintt << "Interrupted before parsing";
        return {};
    }

    simdjson::ondemand::parser parser;
    auto iter_result = parser.iterate(base_ptr, base_size, base_size + padding);
    if (iter_result.error()) {
        qprintt << "Failed to iterate:"
                << simdjson::error_message(iter_result.error());
        return {};
    }
    auto& doc = iter_result.value_unsafe();

    if (parent_pointer.isEmpty() || parent_pointer == "/") {
        // Document root: try object then array directly.
        {
            auto obj_res = doc.get_object();
            if (!obj_res.error()) {
                auto obj = obj_res.value_unsafe();
                try {
                    int idx = 0;
                    for (auto field : obj) {
                        // Check for interruption periodically
                        if (idx % 100 == 0
                            && QThread::currentThread()
                                   ->isInterruptionRequested()) {
                            qprintt << "Interrupted during root object parsing";
                            qDeleteAll(items_vec);
                            return {};
                        }

                        if (range_mode && idx < start) {
                            ++idx;
                            continue;
                        }
                        if (range_mode && idx > end)
                            break;

                        auto fv        = field.value().value();
                        auto raw_tok   = fv.raw_json_token();
                        size_t abs_off = (raw_tok.data() - base_ptr);
                        char first_ch  = raw_tok.empty() ? '?' : raw_tok[0];
                        size_t len;
                        if (first_ch == '{' || first_ch == '[') {
                            auto raw_res = fv.raw_json();
                            len          = raw_res.error()
                                               ? raw_tok.size()
                                               : raw_res.value_unsafe().size();
                        }
                        else {
                            len = raw_tok.size();
                        }
                        std::string_view key = field.unescaped_key();
                        QString key_str
                            = QString::fromUtf8(key.data(), key.size());
                        QString esc_key = key_str;
                        esc_key.replace("~", "~0").replace("/", "~1");
                        auto [vt, vp]
                            = typeAndPreviewFromRaw(base_ptr, abs_off, len);
                        auto* item = new JsonTreeItem(key_str, "/" + esc_key,
                                                      vt, vp, nullptr);
                        item->has_children = hasChildren(vt);
                        item->byte_offset  = abs_off;
                        item->byte_length  = len;
                        items_vec.append(item);
                        ++idx;
                    }
                }
                catch (...) {
                    qprintt << "Exception while parsing object fields";
                    qDeleteAll(items_vec);
                    return {};
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
                            // Check for interruption periodically
                            if (idx % 100 == 0
                                && QThread::currentThread()
                                       ->isInterruptionRequested()) {
                                qprintt
                                    << "Interrupted during root array parsing";
                                qDeleteAll(items_vec);
                                return {};
                            }

                            if (range_mode && idx < start) {
                                ++idx;
                                continue;
                            }
                            if (range_mode && idx > end)
                                break;

                            auto ev        = element.value();
                            auto raw_tok   = ev.raw_json_token();
                            size_t abs_off = (raw_tok.data() - base_ptr);
                            char first_ch  = raw_tok.empty() ? '?' : raw_tok[0];
                            size_t len;
                            if (first_ch == '{' || first_ch == '[') {
                                auto raw_res = ev.raw_json();
                                len          = raw_res.error()
                                                   ? raw_tok.size()
                                                   : raw_res.value_unsafe().size();
                            }
                            else {
                                len = raw_tok.size();
                            }
                            auto [vt, vp]
                                = typeAndPreviewFromRaw(base_ptr, abs_off, len);
                            QString idx_str = QString::number(idx);
                            auto* item      = new JsonTreeItem(
                                idx_str, "/" + idx_str, vt, vp, nullptr);
                            item->has_children = hasChildren(vt);
                            item->byte_offset  = abs_off;
                            item->byte_length  = len;
                            items_vec.append(item);
                            ++idx;
                        }
                    }
                    catch (...) {
                        qprintt << "Exception while parsing array elements";
                        qDeleteAll(items_vec);
                        return {};
                    }
                    return items_vec;
                }
            }
        }
        qprintt << "Failed to parse document root as object or array";
        return items_vec;
    }
    else {
        auto ptr_result = doc.at_pointer(parent_pointer.toStdString());
        if (ptr_result.error()) {
            qprintt << "Failed to navigate to pointer:" << parent_pointer
                    << "Error:" << simdjson::error_message(ptr_result.error());
            return {};
        }
        auto target = ptr_result.value_unsafe();
        return iterateValue(target, parent_pointer, base_ptr, base_ptr, 0,
                            range_mode, start, end);
    }
}
