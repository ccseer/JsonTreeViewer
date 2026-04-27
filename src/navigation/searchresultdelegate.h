#pragma once
#include <QStyledItemDelegate>

class SearchResultDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    static constexpr auto kUserRolePath = Qt::UserRole;
    static constexpr auto kUserRoleKey  = Qt::UserRole + 1;
    static constexpr auto kUserRoleVal  = Qt::UserRole + 2;
    static constexpr auto kUserRoleType = Qt::UserRole + 3;

    explicit SearchResultDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;
};
