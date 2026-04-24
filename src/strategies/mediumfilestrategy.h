#pragma once

#include <simdjson.h>

#include "../memorymappedfile.h"
#include "jsonstrategy.h"

class MediumFileStrategy : public JsonViewerStrategy {
public:
    MediumFileStrategy();
    ~MediumFileStrategy() override;

    bool load(const QString& path) override;
    QVector<JsonTreeItem*> extractChildren(JsonTreeItem* parent_item) override;
    const char* dataPtr() const override;
    size_t dataSize() const override;
    const Metrics& metrics() const override;

private:
    simdjson::padded_string m_json_data;
    Metrics m_metrics;
};
