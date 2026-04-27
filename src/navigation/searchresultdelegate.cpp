#include "searchresultdelegate.h"

#include <QApplication>
#include <QPainter>

SearchResultDelegate::SearchResultDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void SearchResultDelegate::paint(QPainter* painter,
                                 const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    QRect rect    = option.rect;
    bool selected = option.state & QStyle::State_Selected;
    bool hovered  = option.state & QStyle::State_MouseOver;

    // Background
    if (selected) {
        painter->fillRect(rect, option.palette.color(QPalette::Highlight));
    }
    else if (hovered) {
        painter->fillRect(rect, option.palette.color(QPalette::AlternateBase));
    }

    // Border bottom
    painter->setPen(option.palette.color(QPalette::Midlight));
    painter->drawLine(rect.bottomLeft(), rect.bottomRight());

    // Extract data
    QString path = index.data(Qt::UserRole).toString();
    QString text = index.data(Qt::DisplayRole).toString();

    // Splitting key and value from "Key: Value" or "[Path] Value"
    QString key, value;
    if (text.contains(": ")) {
        key   = text.section(": ", 0, 0);
        value = text.section(": ", 1);
    }
    else if (text.startsWith("[")) {
        key   = "";
        value = text.section("] ", 1);
    }
    else {
        key   = text;
        value = "";
    }

    QFont mainFont = option.font;
    QFont pathFont = option.font;
    pathFont.setPointSizeF(mainFont.pointSizeF() * 0.85);

    QColor textColor = option.palette.color(selected ? QPalette::HighlightedText
                                                     : QPalette::Text);
    QColor pathColor = selected
                           ? textColor
                           : option.palette.color(QPalette::PlaceholderText);
    QColor accentColor = selected ? textColor : QColor("#0288D1");

    // Padding
    int px = 12;
    int py = 6;

    // Draw Key (or label)
    painter->setFont(mainFont);
    painter->setPen(textColor);

    QString primaryText = key.isEmpty() ? tr("Value") : key;
    QRect primaryRect   = rect.adjusted(px, py, -px, -rect.height() / 2);
    painter->drawText(primaryRect, Qt::AlignLeft | Qt::AlignVCenter,
                      primaryText);

    // Draw Value snippet
    if (!value.isEmpty()) {
        int labelWidth
            = painter->fontMetrics().horizontalAdvance(primaryText + "  ");
        painter->setPen(accentColor);
        painter->drawText(primaryRect.adjusted(labelWidth, 0, 0, 0),
                          Qt::AlignLeft | Qt::AlignVCenter, value);
    }

    // Draw Path (JSON Pointer)
    painter->setFont(pathFont);
    painter->setPen(pathColor);
    QRect pathRect = rect.adjusted(px, rect.height() / 2, -px, -py);
    painter->drawText(pathRect, Qt::AlignLeft | Qt::AlignVCenter, path);

    painter->restore();
}

QSize SearchResultDelegate::sizeHint(const QStyleOptionViewItem& option,
                                     const QModelIndex& index) const
{
    return QSize(option.rect.width(), 52);
}
