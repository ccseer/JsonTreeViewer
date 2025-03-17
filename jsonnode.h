#pragma once

#include <QVector>

class JsonTreeItem {
public:
    JsonTreeItem(const QString& key,
                 const QString& pos,
                 char type,
                 const QString& value      = QString(),
                 JsonTreeItem* parent_item = nullptr)
        : parent(parent_item),
          key(key),
          value(value),
          pointer(pos),
          type(type),
          has_children(false)
    {
    }
    ~JsonTreeItem()
    {
        qDeleteAll(children);
    }

    QVector<JsonTreeItem*> children;
    JsonTreeItem* parent;

    QString key;
    QString value;

    QString pointer;
    char type;
    bool has_children;
};
