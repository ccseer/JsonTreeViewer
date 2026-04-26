#include "mediumfilestrategy.h"

#include "../jsonnode.h"
#include "../logging.h"

#define qprintt qprint << "[MediumFileStrategy]"

MediumFileStrategy::MediumFileStrategy()  = default;
MediumFileStrategy::~MediumFileStrategy() = default;

bool MediumFileStrategy::initialize(const QString& path)
{
    qprintt << "Loading medium file:" << path;
    auto result = simdjson::padded_string::load(path.toUtf8().data());
    if (result.error()) {
        qprintt << "Failed to load:" << simdjson::error_message(result.error());
        return false;
    }
    m_json_data = std::move(result.value());
    qprintt << "Loaded" << m_json_data.size() << "bytes";
    return true;
}

void MediumFileStrategy::getRootMetadata(QString& pointer,
                                         quint64& byte_offset,
                                         quint64& byte_length,
                                         quint32& child_count)
{
    pointer     = "";  // Root pointer is empty string per RFC 6901
    byte_offset = 0;
    byte_length = m_json_data.size();
    child_count = countLocalBufferChildren(dataPtr(), dataSize());
}

QVector<JsonTreeItem*> MediumFileStrategy::extractChildren(
    const QString& parent_pointer,
    quint64 byte_offset,
    quint64 byte_length,
    int start,
    int end)
{
    return parseLocalBuffer(parent_pointer, dataPtr(), dataSize(), start, end);
}

quint32 MediumFileStrategy::countChildren(const QString& parent_pointer,
                                          quint64 byte_offset,
                                          quint64 byte_length)
{
    return countChildrenAtPointer(parent_pointer, dataPtr(), dataSize());
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
