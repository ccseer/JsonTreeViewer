#pragma once
#include <QStyledItemDelegate>

class SearchResultDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit SearchResultDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;
};
