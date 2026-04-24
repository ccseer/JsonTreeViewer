#include "largefilestrategy.h"

#include <simdjson.h>

#include <QDebug>
#include <cstring>

#include "../jsonnode.h"
#include "../logging.h"

#define qprintt qprint << "[LargeFileStrategy]"

LargeFileStrategy::LargeFileStrategy()  = default;
LargeFileStrategy::~LargeFileStrategy() = default;

bool LargeFileStrategy::load(const QString& path)
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

// simdjson ondemand requires SIMDJSON_PADDING bytes of readable memory beyond
// the end of the document.  The mmap'd region ends at the last file byte, so
// we copy the tail into a small heap buffer that has the required padding.
bool LargeFileStrategy::preparePadding()
{
    const size_t file_size = m_mmap.size();
    const size_t padding   = simdjson::SIMDJSON_PADDING;

    if (file_size == 0)
        return false;

    // simdjson requires SIMDJSON_PADDING readable bytes past the last data
    // byte. Allocate a contiguous buffer = file + padding so iterate() is safe.
    m_padding_buf.resize(file_size + padding, '\0');
    std::memcpy(m_padding_buf.data(), m_mmap.data(), file_size);
    m_data_ptr  = m_padding_buf.data();
    m_data_size = file_size;
    return true;
}

QVector<JsonTreeItem*> LargeFileStrategy::extractChildren(
    JsonTreeItem* parent_item, int start, int end)
{
    return parseLocalBuffer(parent_item, m_data_ptr, m_data_size, start, end);
}

quint32 LargeFileStrategy::countChildren(JsonTreeItem* parent_item)
{
    return countLocalBufferChildren(parent_item, dataPtr(), dataSize());
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
