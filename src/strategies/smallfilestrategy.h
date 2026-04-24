#pragma once

#include <simdjson.h>

#include "jsonstrategy.h"

class SmallFileStrategy : public JsonViewerStrategy {
public:
    SmallFileStrategy();
    ~SmallFileStrategy() override;

    bool load(const QString& path) override;
    QVector<JsonTreeItem*> extractChildren(JsonTreeItem* parent_item) override;
    const char* dataPtr() const override;
    size_t dataSize() const override;
    const Metrics& metrics() const override;

private:
    simdjson::dom::parser m_parser;
    simdjson::dom::element m_root;
    simdjson::padded_string m_json_data;
    Metrics m_metrics;
};
