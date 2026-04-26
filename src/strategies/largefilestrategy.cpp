#include "largefilestrategy.h"

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

    // NOTE: This copies the entire file to add padding, using 2x memory.
    // Trade-off: simdjson requires padding at the end of the buffer, but
    // memory-mapped files cannot guarantee this. Alternatives:
    // 1. Use padded_string::load() - loses mmap benefits, reads entire file
    // 2. Platform-specific tricks (mmap extra pages) - complex, non-portable
    // 3. Current approach - simple, safe, predictable memory usage
    // For 100MB-1GB files, 2x memory is acceptable for correctness.
    m_padding_buf.resize(file_size + padding, '\0');
    std::memcpy(m_padding_buf.data(), m_mmap.data(), file_size);
    m_data_ptr             = m_padding_buf.data();
    m_data_size            = file_size;
    m_metrics.totalBytes   = file_size;
    m_metrics.strategyName = "LargeFileStrategy";
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
    auto result = countLocalBufferChildren(dataPtr(), dataSize());
    child_count = result.count;

    // Store error in metrics if parsing failed
    if (!result.error.isEmpty()) {
        m_metrics.parseError  = result.error;
        m_metrics.errorOffset = result.offset;
        m_metrics.errorContext
            = extractErrorContext(dataPtr(), dataSize(), result.offset);
    }
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

JsonViewerStrategy::CountResult LargeFileStrategy::countChildren(
    const QString& parent_pointer, quint64 byte_offset, quint64 byte_length)
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
