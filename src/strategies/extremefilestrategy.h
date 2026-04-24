#pragma once

#include <simdjson.h>

#include "jsonstrategy.h"

class ExtremeFileStrategy : public JsonViewerStrategy {
public:
    ExtremeFileStrategy();
    ~ExtremeFileStrategy() override;

    bool load(const QString& path) override;
    QVector<JsonTreeItem*> extractChildren(JsonTreeItem* parent_item) override;
    const char* dataPtr() const override;
    size_t dataSize() const override;
    const Metrics& metrics() const override;

private:
    simdjson::padded_string m_json_data;  // Use padded_string instead of mmap
    Metrics m_metrics;
};
