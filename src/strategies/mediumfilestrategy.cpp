#include "mediumfilestrategy.h"

#include <QDebug>

#include "../jsonnode.h"
#include "../logging.h"

#define qprintt qprint << "[MediumFileStrategy]"

MediumFileStrategy::MediumFileStrategy()  = default;
MediumFileStrategy::~MediumFileStrategy() = default;

bool MediumFileStrategy::load(const QString& path)
{
    qprintt << "Loading medium file:" << path;
    auto result = simdjson::padded_string::load(path.toUtf8().data());
    if (result.error()) {
        qprintt << "Failed to load:" << simdjson::error_message(result.error());
        return false;
    }
    m_json_data = std::move(result.value());
    return true;
}

QVector<JsonTreeItem*> MediumFileStrategy::extractChildren(
    JsonTreeItem* parent_item)
{
    return parseLocalBuffer(parent_item, dataPtr(), dataSize());
}

const char* MediumFileStrategy::dataPtr() const
{
    return reinterpret_cast<const char*>(m_json_data.data());
}

size_t MediumFileStrategy::dataSize() const
{
    return m_json_data.size();
}

const JsonViewerStrategy::Metrics& MediumFileStrategy::metrics() const
{
    return m_metrics;
}
