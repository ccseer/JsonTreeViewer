#pragma once

#include <QStyledItemDelegate>

class JsonTreeDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit JsonTreeDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    void initStyleOption(QStyleOptionViewItem* option,
                         const QModelIndex& index) const override;
};
