#include "smallfilestrategy.h"

#include <QDebug>

#include "../jsonnode.h"
#include "../logging.h"

#define qprintt qprint << "[SmallFileStrategy]"

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
    return true;
}

QVector<JsonTreeItem*> SmallFileStrategy::extractChildren(
    JsonTreeItem* parent_item, int start, int end)
{
    return parseLocalBuffer(parent_item, dataPtr(), dataSize(), start, end);
}

quint32 SmallFileStrategy::countChildren(JsonTreeItem* parent_item)
{
    return countLocalBufferChildren(parent_item, dataPtr(), dataSize());
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
