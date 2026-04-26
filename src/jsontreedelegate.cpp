#include "jsontreedelegate.h"

#include <QApplication>
#include <QPainter>

JsonTreeDelegate::JsonTreeDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void JsonTreeDelegate::paint(QPainter* painter,
                             const QStyleOptionViewItem& option,
                             const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    QStyledItemDelegate::paint(painter, opt, index);
}

void JsonTreeDelegate::initStyleOption(QStyleOptionViewItem* option,
                                       const QModelIndex& index) const
{
    QStyledItemDelegate::initStyleOption(option, index);

    if (option->state & QStyle::State_Selected) {
        // When selected, force the text color to the highlighted text color
        option->palette.setColor(
            QPalette::Text, option->palette.color(QPalette::HighlightedText));
    }
}
