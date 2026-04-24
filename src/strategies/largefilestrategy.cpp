#include "largefilestrategy.h"
#include "../jsonnode.h"
#include "../logging.h"

#include <QDebug>

#define qprintt qprint << "[LargeFileStrategy]"

LargeFileStrategy::LargeFileStrategy() = default;

LargeFileStrategy::~LargeFileStrategy() = default;

bool LargeFileStrategy::load(const QString& path)
{
    qprintt << "Loading large file:" << path;

    // Use simdjson's padded_string which handles padding automatically
    auto result = simdjson::padded_string::load(path.toStdString());
    if (result.error()) {
        qprintt << "Failed to load file:" << simdjson::error_message(result.error());
        return false;
    }

    m_json_data = std::move(result.value());
    qprintt << "Successfully loaded" << m_json_data.size() << "bytes with padding";
    return true;
}

QVector<JsonTreeItem*> LargeFileStrategy::extractChildren(JsonTreeItem* parent_item)
{
    return parseLocalBuffer(parent_item, dataPtr(), dataSize());
}

const char* LargeFileStrategy::dataPtr() const
{
    return m_json_data.data();
}

size_t LargeFileStrategy::dataSize() const
{
    return m_json_data.size();
}

const JsonViewerStrategy::Metrics& LargeFileStrategy::metrics() const
{
    return m_metrics;
}
