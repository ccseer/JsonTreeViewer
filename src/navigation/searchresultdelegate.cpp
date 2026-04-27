#include "searchresultdelegate.h"

#include <QPainter>
#include <QScreen>

#include "../style_assets.h"

using namespace jtv::ui;

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

    // Border bottom (very subtle)
    QColor borderColor = option.palette.color(QPalette::Midlight);
    borderColor.setAlpha(80);
    painter->setPen(borderColor);
    painter->drawLine(rect.bottomLeft(), rect.bottomRight());

    // Extract data from custom roles
    const QString path  = index.data(kUserRolePath).toString();
    const QString key   = index.data(kUserRoleKey).toString();
    const QString value = index.data(kUserRoleVal).toString();
    const QString type  = index.data(kUserRoleType).toString();

    // Visual configuration
    qreal dpr = 1;
    if (option.widget && option.widget->screen()) {
        dpr = option.widget->screen()->logicalDotsPerInch() * 1. / 96.0;
    }
    int px       = qRound(12 * dpr);
    int py       = qRound(4 * dpr);
    int iconSize = qRound(14 * dpr);

    bool isDarkMode = true;
    if (option.widget) {
        isDarkMode
            = option.widget->palette().color(QPalette::Window).lightness()
              < 128;
    }

    QColor textColor   = selected
                             ? option.palette.color(QPalette::HighlightedText)
                             : QColor(isDarkMode ? jtv::ui::Colors::DarkText
                                                 : jtv::ui::Colors::LightText);
    QColor pathColor   = selected
                             ? textColor
                             : QColor(isDarkMode ? jtv::ui::Colors::DarkTextDim
                                                 : jtv::ui::Colors::LightTextDim);
    QColor accentColor = selected ? textColor : jtv::ui::Colors::Accent;

    // Draw Icon based on type
    const char* svg_data = g_svg_article;  // default string
    if (type == "o")
        svg_data = g_svg_object;
    else if (type == "a")
        svg_data = g_svg_array;

    QIcon icon = svgIcon(svg_data, accentColor, 14, dpr);
    QRect iconRect(rect.left() + px,
                   rect.top() + (rect.height() - iconSize) / 2, iconSize,
                   iconSize);
    icon.paint(painter, iconRect);

    // Layout
    int contentX = iconRect.right() + qRound(8 * dpr);
    int labelY   = rect.top() + py;

    // Draw Key (Medium weight)
    QFont keyFont = option.font;
    keyFont.setWeight(QFont::Medium);
    painter->setFont(keyFont);
    painter->setPen(textColor);

    QString keyText = key.isEmpty() ? tr("Value") : key;
    int keyWidth    = painter->fontMetrics().horizontalAdvance(keyText);
    QRect keyRect(contentX, labelY, keyWidth, rect.height() / 2);
    painter->drawText(keyRect, Qt::AlignLeft | Qt::AlignTop, keyText);

    // Draw Value (Accent)
    if (!value.isEmpty()) {
        painter->setPen(accentColor);
        int spacing = qRound(8 * dpr);
        QRect valRect(keyRect.right() + spacing, labelY,
                      rect.width() - keyRect.right() - px - spacing,
                      rect.height() / 2);
        QString valText = "  " + value;
        painter->drawText(valRect, Qt::AlignLeft | Qt::AlignTop, valText);
    }

    // Draw Path (Dimmed, Mono-ish if possible)
    QFont pathFont = option.font;
    if (pathFont.pointSizeF() > 0) {
        pathFont.setPointSizeF(pathFont.pointSizeF() * 0.8);
    }
    else {
        pathFont.setPixelSize(qMax(1, qRound(pathFont.pixelSize() * 0.8)));
    }
    painter->setFont(pathFont);
    painter->setPen(pathColor);

    QRect pathRect(contentX, rect.top() + rect.height() / 2,
                   rect.width() - contentX - px, rect.height() / 2 - py);
    painter->drawText(pathRect, Qt::AlignLeft | Qt::AlignVCenter, path);

    painter->restore();
}

QSize SearchResultDelegate::sizeHint(const QStyleOptionViewItem& option,
                                     const QModelIndex& index) const
{
    qreal dpr = 1.0;
    if (option.widget && option.widget->screen()) {
        dpr = option.widget->screen()->logicalDotsPerInch() * 1. / 96.0;
    }
    return QSize(option.rect.width(), qRound(40 * dpr));
}
