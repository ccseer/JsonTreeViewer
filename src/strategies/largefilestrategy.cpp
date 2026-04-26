#include "largefilestrategy.h"

#include "../logging.h"

#define qprintt qprint << "[LargeFileStrategy]"

LargeFileStrategy::LargeFileStrategy()  = default;
LargeFileStrategy::~LargeFileStrategy() = default;

bool LargeFileStrategy::initialize(const QString& path)
{
    qprintt << "Loading large file (mmap):" << path;

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

bool LargeFileStrategy::preparePadding()
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

void LargeFileStrategy::getRootMetadata(QString& pointer,
                                        quint64& byte_offset,
                                        quint64& byte_length,
                                        quint32& child_count)
{
    pointer     = "";  // Root pointer is empty string per RFC 6901
    byte_offset = 0;
    byte_length = m_data_size;
    child_count = countLocalBufferChildren(dataPtr(), dataSize());
}

QVector<JsonTreeItem*> LargeFileStrategy::extractChildren(
    const QString& parent_pointer,
    quint64 byte_offset,
    quint64 byte_length,
    int start,
    int end)
{
    return parseLocalBuffer(parent_pointer, m_data_ptr, m_data_size, start,
                            end);
}

quint32 LargeFileStrategy::countChildren(const QString& parent_pointer,
                                         quint64 byte_offset,
                                         quint64 byte_length)
{
    return countChildrenAtPointer(parent_pointer, dataPtr(), dataSize());
}

const char* LargeFileStrategy::dataPtr() const
{
    return m_data_ptr;
}

size_t LargeFileStrategy::dataSize() const
{
    return m_data_size;
}

const JsonViewerStrategy::Metrics& LargeFileStrategy::metrics() const
{
    return m_metrics;
}
