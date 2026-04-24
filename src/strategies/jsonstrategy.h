#pragma once

#include <simdjson.h>

#include <QPair>
#include <QString>
#include <QVector>

class JsonTreeItem;

// Strategy selection thresholds
namespace StrategyThresholds {
    constexpr qint64 SMALL_FILE_MAX   = 10 * 1024 * 1024;      // 10 MB
    constexpr qint64 MEDIUM_FILE_MAX  = 100 * 1024 * 1024;    // 100 MB
    constexpr qint64 LARGE_FILE_MAX   = 1024 * 1024 * 1024LL; // 1 GB
}

class JsonViewerStrategy {
public:
    virtual ~JsonViewerStrategy() = default;

    virtual bool load(const QString& path) = 0;
    virtual QVector<JsonTreeItem*> extractChildren(JsonTreeItem* parent_item)
        = 0;
    virtual const char* dataPtr() const = 0;
    virtual size_t dataSize() const     = 0;

    struct Metrics {
        qint64 parseTimeMs      = 0;
        qint64 treeBuildTimeMs  = 0;
        qint64 totalLoadTimeMs  = 0;
        qint64 memoryUsageBytes = 0;
    };
    virtual const Metrics& metrics() const = 0;

protected:
    // Shared local-parse logic used by Medium, Large and Extreme strategies.
    // Parses only the byte range recorded in parent_item, returns child items.
    static QVector<JsonTreeItem*> parseLocalBuffer(JsonTreeItem* parent_item,
                                                   const char* base_ptr,
                                                   size_t base_size);

    // Raw-byte type detection shared by all ondemand-based strategies.
    static QPair<char, QString> typeAndPreviewFromRaw(const char* data_ptr,
                                                      size_t offset,
                                                      size_t length);

    static bool hasChildren(char type)
    {
        return type == '{' || type == '[';
    }
};
