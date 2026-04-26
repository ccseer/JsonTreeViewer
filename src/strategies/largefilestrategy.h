#pragma once

#include "../memorymappedfile.h"
#include "jsonstrategy.h"

class LargeFileStrategy : public JsonViewerStrategy {
public:
    LargeFileStrategy();
    ~LargeFileStrategy() override;

    FileMode type() const override
    {
        return FileMode::Large;
    }

    bool initialize(const QString& path) override;

    void getRootMetadata(QString& pointer,
                         quint64& byte_offset,
                         quint64& byte_length,
                         quint32& child_count) override;

    QVector<JsonTreeItem*> extractChildren(const QString& parent_pointer,
                                           quint64 byte_offset,
                                           quint64 byte_length,
                                           int start = -1,
                                           int end   = -1) override;

    quint32 countChildren(const QString& parent_pointer,
                          quint64 byte_offset,
                          quint64 byte_length) override;

    const char* dataPtr() const override;
    size_t dataSize() const override;
    const Metrics& metrics() const override;

    CopyActions supportedActions() const override
    {
        return CopyAction::Key | CopyAction::Value | CopyAction::Path;
    }

private:
    MemoryMappedFile m_mmap;
    std::vector<char> m_padding_buf;
    const char* m_data_ptr = nullptr;
    size_t m_data_size     = 0;
    Metrics m_metrics;

    bool preparePadding();
};
