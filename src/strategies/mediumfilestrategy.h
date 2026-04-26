#pragma once

#include <simdjson.h>

#include "jsonstrategy.h"

class MediumFileStrategy : public JsonViewerStrategy {
public:
    MediumFileStrategy();
    ~MediumFileStrategy() override;

    FileMode type() const override
    {
        return FileMode::Medium;
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
        return CopyAction::Key | CopyAction::Value | CopyAction::Path
               | CopyAction::KeyValue | CopyAction::Subtree;
    }

private:
    simdjson::padded_string m_json_data;
    Metrics m_metrics;
};
