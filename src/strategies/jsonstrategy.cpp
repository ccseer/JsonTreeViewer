#include "jsonstrategy.h"

#include <QDebug>
#include <QThread>
#include <limits>

#include "../config.h"
#include "../jsonnode.h"
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
        return {'{', "Object"};
    if (first == '[')
        return {'[', "Array"};
    if (first == '"') {
        // String: raw_tok includes quotes, so we need to skip them
        // Check if we actually have both opening and closing quotes
        if (length < 2)
            return {'s', {}};

        // Find the actual closing quote by properly handling escape sequences
        size_t content_len = 0;
        for (size_t i = 1; i < length; ++i) {
            if (data_ptr[offset + i] == '"') {
                // Count consecutive backslashes before this quote
                size_t backslash_count = 0;
                for (size_t j = i - 1; j > 0 && data_ptr[offset + j] == '\\';
                     --j) {
                    backslash_count++;
                }
                // If backslash count is even (including 0), this quote is not
                // escaped
                if (backslash_count % 2 == 0) {
                    content_len = i - 1;
                    break;
                }
            }
        }

        if (content_len == 0) {
            // Didn't find closing quote, use length - 2 as fallback
            content_len = length > 2 ? length - 2 : 0;
        }

        // Enhanced preview: show truncated content with character count for
        // long strings
        if (content_len > 80) {
            QString preview = QString::fromUtf8(data_ptr + offset + 1, 80);
            preview.replace("\n", " ");
            return {
                's',
                QString("\"%1...\" (%2 chars)").arg(preview).arg(content_len)};
        }
        // Return string content with quotes for small strings
        return {'s', "\""
                         + QString::fromUtf8(data_ptr + offset + 1,
                                             static_cast<int>(content_len))
                         + "\""};
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

                auto fv_res = field.value();
                if (fv_res.error()) {
                    qprintt << "Field value error at index" << idx << ":"
                            << simdjson::error_message(fv_res.error());
                    break;  // Stop iteration - ondemand iterator state is
                            // corrupted
                }
                auto fv = fv_res.value_unsafe();

                auto raw_tok = fv.raw_json_token();

                size_t abs_off = (raw_tok.data() - parse_ptr) + base_off;
                char first_ch  = raw_tok.empty() ? '?' : raw_tok[0];
                size_t len;
                if (first_ch == '{' || first_ch == '[') {
                    auto raw_res = fv.raw_json();
                    len          = raw_res.value_unsafe().size();
                }
                else {
                    len = raw_tok.size();
                }
                auto [vt, vp] = typeAndPreviewFromRaw(base_ptr, abs_off, len);

                auto key_res = field.unescaped_key();
                if (key_res.error()) {
                    qprintt << "Field key error at index" << idx << ":"
                            << simdjson::error_message(key_res.error());
                    break;  // Stop iteration - ondemand iterator state is
                            // corrupted
                }
                std::string_view key = key_res.value_unsafe();
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

                auto ev_res = element;
                if (ev_res.error()) {
                    qprintt << "Array element error at index" << idx << ":"
                            << simdjson::error_message(ev_res.error());
                    break;  // Stop iteration - ondemand iterator state is
                            // corrupted
                }
                auto ev = ev_res.value_unsafe();

                auto raw_tok = ev.raw_json_token();

                size_t abs_off = (raw_tok.data() - parse_ptr) + base_off;
                char first_ch  = raw_tok.empty() ? '?' : raw_tok[0];
                size_t len;
                if (first_ch == '{' || first_ch == '[') {
                    auto raw_res = ev.raw_json();
                    len          = raw_res.value_unsafe().size();
                }
                else {
                    len = raw_tok.size();
                }

                auto [vt, vp] = typeAndPreviewFromRaw(base_ptr, abs_off, len);
                // Display index (may start from 1 if configured)
                int display_idx
                    = idx
                      + (Config::instance().arrayIndexStartsAtZero() ? 0 : 1);
                QString idx_str = QString::number(display_idx);
                // JSON Pointer always uses actual index (0-based)
                auto* item = new JsonTreeItem(
                    idx_str, parent_pointer + "/" + QString::number(idx), vt,
                    vp,
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

JsonViewerStrategy::CountResult JsonViewerStrategy::countLocalBufferChildren(
    const char* base_ptr, size_t base_size)
{
    const size_t padding = simdjson::SIMDJSON_PADDING;

    simdjson::ondemand::parser parser;
    auto iter_result = parser.iterate(base_ptr, base_size, base_size + padding);
    if (iter_result.error()) {
        QString error
            = QString::fromUtf8(simdjson::error_message(iter_result.error()));

        // Use ondemand to find the exact error location
        simdjson::ondemand::parser loc_parser;
        auto loc_res
            = loc_parser.iterate(base_ptr, base_size, base_size + padding);
        quint64 offset = 0;

        if (!loc_res.error()) {
            auto& doc = loc_res.value_unsafe();
            // Try to get the initial value to trigger the error and find its
            // location
            auto val_res = doc.get_value();
            if (val_res.error()) {
                auto pos_res = doc.current_location();
                if (!pos_res.error()) {
                    const char* pos = pos_res.value_unsafe();
                    if (pos >= base_ptr && pos <= base_ptr + base_size) {
                        offset = static_cast<quint64>(pos - base_ptr);
                    }
                }
            }
        }
        else {
            // If iteration failed immediately, try to get location from the
            // result
            auto pos_res = loc_res.current_location();
            if (!pos_res.error()) {
                const char* pos = pos_res.value_unsafe();
                if (pos >= base_ptr && pos <= base_ptr + base_size) {
                    offset = static_cast<quint64>(pos - base_ptr);
                }
            }
        }

        qprintt << "JSON parse error:" << error << "at exact offset:" << offset;
        return {0, error, offset};
    }
    auto& doc = iter_result.value_unsafe();

    // Determine type and count children
    auto val_res = doc.get_value();
    if (val_res.error()) {
        // This is where we catch errors that happen right at the start
        return {0, QString::fromUtf8(simdjson::error_message(val_res.error())),
                0};
    }

    auto val      = val_res.value_unsafe();
    auto type_res = val.type();
    if (type_res.error()) {
        return {0, QString::fromUtf8(simdjson::error_message(type_res.error())),
                0};
    }

    auto type = type_res.value_unsafe();
    if (type == simdjson::ondemand::json_type::object) {
        auto obj_res = val.get_object();
        if (obj_res.error()) {
            // Fallback error location
            return {0,
                    QString::fromUtf8(simdjson::error_message(obj_res.error())),
                    0};
        }
        auto count_res = obj_res.value_unsafe().count_fields();
        if (count_res.error()) {
            // CRITICAL: Catch errors during field counting (deep syntax errors)
            QString error
                = QString::fromUtf8(simdjson::error_message(count_res.error()));
            quint64 offset = 0;
            auto pos_res   = doc.current_location();
            if (!pos_res.error()) {
                const char* pos = pos_res.value_unsafe();
                if (pos >= base_ptr && pos <= base_ptr + base_size) {
                    offset = static_cast<quint64>(pos - base_ptr);
                }
            }
            return {0, error, offset};
        }
        return {static_cast<quint32>(count_res.value_unsafe()), QString(), 0};
    }
    else if (type == simdjson::ondemand::json_type::array) {
        auto arr_res = val.get_array();
        if (arr_res.error()) {
            return {0,
                    QString::fromUtf8(simdjson::error_message(arr_res.error())),
                    0};
        }
        auto count_res = arr_res.value_unsafe().count_elements();
        if (count_res.error()) {
            // CRITICAL: Catch errors during element counting
            QString error
                = QString::fromUtf8(simdjson::error_message(count_res.error()));
            quint64 offset = 0;
            auto pos_res   = doc.current_location();
            if (!pos_res.error()) {
                const char* pos = pos_res.value_unsafe();
                if (pos >= base_ptr && pos <= base_ptr + base_size) {
                    offset = static_cast<quint64>(pos - base_ptr);
                }
            }
            return {0, error, offset};
        }
        return {static_cast<quint32>(count_res.value_unsafe()), QString(), 0};
    }

    return {0, QString(), 0};
}

JsonViewerStrategy::CountResult JsonViewerStrategy::countChildrenAtPointer(
    const QString& parent_pointer, const char* base_ptr, size_t base_size)
{
    if (parent_pointer.isEmpty()) {
        return countLocalBufferChildren(base_ptr, base_size);
    }

    // Navigate to the parent node using JSON pointer
    const size_t padding = simdjson::SIMDJSON_PADDING;
    simdjson::ondemand::parser parser;
    auto iter_result = parser.iterate(base_ptr, base_size, base_size + padding);
    if (iter_result.error())
        return {0,
                QString::fromUtf8(simdjson::error_message(iter_result.error())),
                0};

    auto& doc         = iter_result.value_unsafe();
    auto value_result = doc.at_pointer(parent_pointer.toStdString());
    if (value_result.error())
        return {
            0, QString::fromUtf8(simdjson::error_message(value_result.error())),
            0};

    auto value = value_result.value_unsafe();

    // Count children of this value
    auto obj_res = value.get_object();
    if (!obj_res.error()) {
        auto count_res = obj_res.value_unsafe().count_fields();
        if (!count_res.error())
            return {static_cast<quint32>(count_res.value_unsafe()), QString(),
                    0};
    }

    auto arr_res = value.get_array();
    if (!arr_res.error()) {
        auto count_res = arr_res.value_unsafe().count_elements();
        if (!count_res.error())
            return {static_cast<quint32>(count_res.value_unsafe()), QString(),
                    0};
    }

    return {0, QString(), 0};
}

QString JsonViewerStrategy::extractErrorContext(const char* base_ptr,
                                                size_t base_size,
                                                quint64 offset)
{
    if (!base_ptr || base_size == 0)
        return QString();

    // Limit offset to stay within bounds
    offset = qMin(offset, static_cast<quint64>(base_size - 1));

    constexpr int context_size = 50;
    int start = qMax(0, static_cast<int>(offset) - context_size);
    int end   = qMin(static_cast<int>(base_size),
                     static_cast<int>(offset) + context_size);

    QString context;
    if (start > 0)
        context += "...";

    // Read the chunk and handle non-printable characters or newlines
    QByteArray chunk(base_ptr + start, end - start);
    QString text = QString::fromUtf8(chunk);
    text.replace('\n', ' ').replace('\r', ' ');
    context += text;

    if (end < static_cast<int>(base_size))
        context += "...";

    return context;
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

                        auto fv_res = field.value();
                        if (fv_res.error()) {
                            qprintt << "Root object field value error at index"
                                    << idx << ":"
                                    << simdjson::error_message(fv_res.error());
                            break;  // Stop iteration - ondemand iterator state
                                    // is corrupted
                        }
                        auto fv = fv_res.value_unsafe();

                        auto raw_tok = fv.raw_json_token();

                        size_t abs_off = (raw_tok.data() - base_ptr);
                        char first_ch  = raw_tok.empty() ? '?' : raw_tok[0];
                        size_t len;
                        if (first_ch == '{' || first_ch == '[') {
                            auto raw = fv.raw_json();
                            len      = raw.value_unsafe().size();
                        }
                        else {
                            len = raw_tok.size();
                        }
                        auto key_res = field.unescaped_key();
                        if (key_res.error()) {
                            qprintt << "Root object field key error at index"
                                    << idx << ":"
                                    << simdjson::error_message(key_res.error());
                            break;  // Stop iteration - ondemand iterator state
                                    // is corrupted
                        }
                        std::string_view key = key_res.value_unsafe();
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

                            auto ev_res = element;
                            if (ev_res.error()) {
                                qprintt
                                    << "Root array element error at index"
                                    << idx << ":"
                                    << simdjson::error_message(ev_res.error());
                                break;  // Stop iteration - ondemand iterator
                                        // state is corrupted
                            }
                            auto ev = ev_res.value_unsafe();

                            auto raw_tok = ev.raw_json_token();

                            size_t abs_off = (raw_tok.data() - base_ptr);
                            char first_ch  = raw_tok.empty() ? '?' : raw_tok[0];
                            size_t len;
                            if (first_ch == '{' || first_ch == '[') {
                                auto raw = ev.raw_json();
                                len      = raw.value_unsafe().size();
                            }
                            else {
                                len = raw_tok.size();
                            }
                            auto [vt, vp]
                                = typeAndPreviewFromRaw(base_ptr, abs_off, len);
                            // Display index (may start from 1 if configured)
                            int display_idx
                                = idx
                                  + (Config::instance().arrayIndexStartsAtZero()
                                         ? 0
                                         : 1);
                            QString idx_str = QString::number(display_idx);
                            // JSON Pointer always uses actual index (0-based)
                            auto* item = new JsonTreeItem(
                                idx_str, "/" + QString::number(idx), vt, vp,
                                nullptr);
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
