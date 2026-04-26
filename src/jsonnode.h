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

    // Byte offset and length caching for performance
    quint64 byte_offset;  // Starting byte position of this node in the original
                          // JSON file
    quint64 byte_length;  // Total byte length of this node INCLUDING all its
                          // children in the original JSON Example: for {"a":
                          // {"b": 1}}, the byte_length of "a" includes the
                          // entire {"b": 1} subtree
    quint32 child_count;   // Number of direct children (for paging decisions)
    bool children_loaded;  // Whether children have been loaded from disk/parsed

    // Virtual paging support
    bool is_virtual_page = false;  // Is this a virtual page node?
    int page_start       = -1;     // Start index of page range
    int page_end         = -1;     // End index of page range (inclusive)

    // Helper: Check if this is a loading placeholder
    // Loading placeholders have: "Loading..." key, empty pointer, type=0,
    // !has_children Multiple checks for safety
    bool isLoadingPlaceholder() const
    {
        return key.startsWith("Loading") && pointer.isEmpty() && type == 0
               && !has_children;
    }
};
