#pragma once

#include <simdjson.h>

#include <QSortFilterProxyModel>
#include <memory>

class JsonTreeItem;
class JsonViewerStrategy;

class JsonTreeModel : public QAbstractItemModel {
    Q_OBJECT
public:
    enum class FileMode { Small, Medium, Large, Extreme };

    explicit JsonTreeModel(QObject* parent = nullptr);
    ~JsonTreeModel() override;

    static QString toEscaped(const QString& key);
    bool load(const QString& path);
    void loadEverything();

    FileMode fileMode() const
    {
        return m_file_mode;
    }
    // QModelIndex navigateToPath(const QString& path);

    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section,
                        Qt::Orientation o,
                        int role) const override;
    QModelIndex index(int row,
                      int column,
                      const QModelIndex& parent = QModelIndex()) const override;

    bool hasChildren(const QModelIndex& parent) const override;
    QModelIndex parent(const QModelIndex& index) const override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& = QModelIndex()) const override;

    bool canFetchMore(const QModelIndex& parent) const override;
    void fetchMore(const QModelIndex& parent) override;

private:
    JsonTreeItem* getItem(const QModelIndex& index) const;
    QVector<JsonTreeItem*> extractChildren(JsonTreeItem* parent_item);

    // Paging support
    int getPageSize(FileMode mode) const;
    bool needsPaging(int child_count, FileMode mode) const;
    QVector<JsonTreeItem*> createPagedChildren(JsonTreeItem* parent_item,
                                               int total_children);

    JsonTreeItem* m_root_item;
    std::unique_ptr<JsonViewerStrategy> m_strategy;
    FileMode m_file_mode;

    // Legacy members - will be removed after full strategy integration
    simdjson::ondemand::parser m_parser;
    simdjson::padded_string m_json_data;
    simdjson::ondemand::document m_doc;
};

////////////////////////////////////////////////////////////
class TreeFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    TreeFilterProxyModel(QObject* parent = nullptr);

    void updateFilter(const QString& text);

protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override;
    QString m_filter_text;
};
