#pragma once

#include <simdjson.h>

#include <QPair>
#include <QString>
#include <QVector>

class JsonTreeItem;

namespace StrategyThresholds {
constexpr qint64 SMALL_FILE_MAX  = 10 * 1024 * 1024;
constexpr qint64 MEDIUM_FILE_MAX = 100 * 1024 * 1024;
constexpr qint64 LARGE_FILE_MAX  = 1024 * 1024 * 1024LL;
}  // namespace StrategyThresholds

class JsonViewerStrategy {
public:
    virtual ~JsonViewerStrategy() = default;

    virtual bool load(const QString& path) = 0;

    // start/end >= 0: return only children with index in [start, end].
    // Pass -1 / -1 (defaults) to get all children.
    virtual QVector<JsonTreeItem*> extractChildren(JsonTreeItem* parent_item,
                                                   int start = -1,
                                                   int end   = -1)
        = 0;
    virtual quint32 countChildren(JsonTreeItem* parent_item) = 0;

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
    static quint32 countLocalBufferChildren(JsonTreeItem* parent_item,
                                            const char* base_ptr,
                                            size_t base_size);

    // start/end: same range semantics as extractChildren.
    static QVector<JsonTreeItem*> parseLocalBuffer(JsonTreeItem* parent_item,
                                                   const char* base_ptr,
                                                   size_t base_size,
                                                   int start = -1,
                                                   int end   = -1);
};
