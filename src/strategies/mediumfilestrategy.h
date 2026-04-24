#pragma once

#include <simdjson.h>

#include "jsonstrategy.h"

class MediumFileStrategy : public JsonViewerStrategy {
public:
    MediumFileStrategy();
    ~MediumFileStrategy() override;

    bool load(const QString& path) override;
    QVector<JsonTreeItem*> extractChildren(JsonTreeItem* parent_item,
                                           int start = -1,
                                           int end   = -1) override;
    quint32 countChildren(JsonTreeItem* parent_item) override;
    const char* dataPtr() const override;
    size_t dataSize() const override;
    const Metrics& metrics() const override;

private:
    simdjson::padded_string m_json_data;
    Metrics m_metrics;
};
