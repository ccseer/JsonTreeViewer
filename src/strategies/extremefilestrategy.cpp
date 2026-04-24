#include "extremefilestrategy.h"
#include "../jsonnode.h"
#include "../logging.h"

#include <QDebug>

#define qprintt qprint << "[ExtremeFileStrategy]"

ExtremeFileStrategy::ExtremeFileStrategy() = default;

ExtremeFileStrategy::~ExtremeFileStrategy() = default;

bool ExtremeFileStrategy::load(const QString& path)
{
    qprintt << "Loading extreme file:" << path;

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

QVector<JsonTreeItem*> ExtremeFileStrategy::extractChildren(JsonTreeItem* parent_item)
{
    return parseLocalBuffer(parent_item, dataPtr(), dataSize());
}

const char* ExtremeFileStrategy::dataPtr() const
{
    return m_json_data.data();
}

size_t ExtremeFileStrategy::dataSize() const
{
    return m_json_data.size();
}

const JsonViewerStrategy::Metrics& ExtremeFileStrategy::metrics() const
{
    return m_metrics;
}
