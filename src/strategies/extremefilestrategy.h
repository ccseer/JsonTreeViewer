#pragma once

#include "../memorymappedfile.h"
#include "jsonstrategy.h"

class ExtremeFileStrategy : public JsonViewerStrategy {
public:
    ExtremeFileStrategy();
    ~ExtremeFileStrategy() override;

    bool load(const QString& path) override;
    QVector<JsonTreeItem*> extractChildren(JsonTreeItem* parent_item,
                                           int start = -1,
                                           int end   = -1) override;
    quint32 countChildren(JsonTreeItem* parent_item) override;
    const char* dataPtr() const override;
    size_t dataSize() const override;
    const Metrics& metrics() const override;
    CopyActions supportedActions() const override
    {
        return CopyAction::Key | CopyAction::Value;
    }

private:
    MemoryMappedFile m_mmap;
    std::vector<char> m_padding_buf;
    const char* m_data_ptr = nullptr;
    size_t m_data_size     = 0;
    Metrics m_metrics;

    bool preparePadding();
};
