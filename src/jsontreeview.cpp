#include "jsontreeview.h"

#include <QTimer>

JsonTreeView::JsonTreeView(QWidget *parent) : QTreeView(parent)
{
    setUniformRowHeights(true);
    setAnimated(false);
}

void JsonTreeView::upadteDPR(qreal r)
{
    setIndentation(22 * r);
    scheduleDelayedItemsLayout();
    QTimer::singleShot(0, this,
                       [this]() { this->scheduleDelayedItemsLayout(); });
}
