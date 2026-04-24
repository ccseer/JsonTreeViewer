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
          has_children(false),
          byte_offset(0),
          byte_length(0),
          child_count(0),
          children_loaded(false)
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

    // Byte offset caching for performance
    quint64 byte_offset;      // Node's byte offset in file
    quint64 byte_length;      // Node's byte length
    quint32 child_count;      // Number of children (for paging)
    bool children_loaded;     // Whether children have been loaded
};
