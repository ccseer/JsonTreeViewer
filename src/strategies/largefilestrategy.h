#pragma once

#include "../memorymappedfile.h"
#include "jsonstrategy.h"

class LargeFileStrategy : public JsonViewerStrategy {
public:
    LargeFileStrategy();
    ~LargeFileStrategy() override;

    bool load(const QString& path) override;
    QVector<JsonTreeItem*> extractChildren(JsonTreeItem* parent_item,
                                           int start = -1,
                                           int end   = -1) override;
    quint32 countChildren(JsonTreeItem* parent_item) override;
    const char* dataPtr() const override;
    size_t dataSize() const override;
    const Metrics& metrics() const override;

private:
    MemoryMappedFile m_mmap;
    // simdjson requires SIMDJSON_PADDING bytes after the last byte it reads.
    // mmap gives us the raw file bytes; we hold a padded copy only for the
    // padding region at the very end (< 64 bytes allocated on the heap).
    std::vector<char> m_padding_buf;
    const char* m_data_ptr = nullptr;
    size_t m_data_size     = 0;
    Metrics m_metrics;

    bool preparePadding();
};
