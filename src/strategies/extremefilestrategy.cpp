#include "extremefilestrategy.h"

#include "../logging.h"

#define qprintt qprint << "[ExtremeFileStrategy]"

ExtremeFileStrategy::ExtremeFileStrategy()  = default;
ExtremeFileStrategy::~ExtremeFileStrategy() = default;

bool ExtremeFileStrategy::initialize(const QString& path)
{
    qprintt << "Loading extreme file (mmap):" << path;

    if (!m_mmap.map(path)) {
        qprintt << "MemoryMappedFile::map failed";
        return false;
    }

    if (!preparePadding()) {
        m_mmap.unmap();
        return false;
    }

    qprintt << "Successfully mapped" << m_data_size << "bytes";
    return true;
}

bool ExtremeFileStrategy::preparePadding()
{
    const size_t file_size = m_mmap.size();
    const size_t padding   = simdjson::SIMDJSON_PADDING;
    if (file_size == 0)
        return false;

    m_padding_buf.resize(file_size + padding, '\0');
    std::memcpy(m_padding_buf.data(), m_mmap.data(), file_size);
    m_data_ptr  = m_padding_buf.data();
    m_data_size = file_size;
    return true;
}

void ExtremeFileStrategy::getRootMetadata(QString& pointer,
                                          quint64& byte_offset,
                                          quint64& byte_length,
                                          quint32& child_count)
{
    pointer     = "";  // Root pointer is empty string per RFC 6901
    byte_offset = 0;
    byte_length = m_data_size;
    child_count = countLocalBufferChildren(dataPtr(), dataSize());
}

QVector<JsonTreeItem*> ExtremeFileStrategy::extractChildren(
    const QString& parent_pointer,
    quint64 byte_offset,
    quint64 byte_length,
    int start,
    int end)
{
    return parseLocalBuffer(parent_pointer, m_data_ptr, m_data_size, start,
                            end);
}

quint32 ExtremeFileStrategy::countChildren(const QString& parent_pointer,
                                           quint64 byte_offset,
                                           quint64 byte_length)
{
    return countLocalBufferChildren(dataPtr(), dataSize());
}

const char* ExtremeFileStrategy::dataPtr() const
{
    return m_data_ptr;
}

size_t ExtremeFileStrategy::dataSize() const
{
    return m_data_size;
}

const JsonViewerStrategy::Metrics& ExtremeFileStrategy::metrics() const
{
    return m_metrics;
}
