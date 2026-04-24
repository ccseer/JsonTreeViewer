#include "jsontreeview.h"

#include <QContextMenuEvent>
#include <QMenu>
#include <QTimer>

JsonTreeView::JsonTreeView(QWidget *parent) : QTreeView(parent)
{
    setUniformRowHeights(true);
    setAnimated(false);
    setContextMenuPolicy(Qt::DefaultContextMenu);
}

void JsonTreeView::upadteDPR(qreal r)
{
    if (!isVisible()) {
        return;
    }
    setIndentation(22 * r);
    scheduleDelayedItemsLayout();
    QTimer::singleShot(0, this,
                       [this]() { this->scheduleDelayedItemsLayout(); });
}

void JsonTreeView::contextMenuEvent(QContextMenuEvent* event)
{
    QModelIndex index = indexAt(event->pos());
    if (!index.isValid()) {
        return;
    }

    QMenu menu(this);

    QAction* copyKeyAction = menu.addAction(tr("Copy Key"));
    QAction* copyValueAction = menu.addAction(tr("Copy Value"));
    QAction* copyPathAction = menu.addAction(tr("Copy Path (JSON Pointer)"));
    menu.addSeparator();
    QAction* copySubtreeAction = menu.addAction(tr("Copy Subtree"));

    QAction* selected = menu.exec(event->globalPos());

    if (selected == copyKeyAction) {
        emit copyKeyRequested(index);
    } else if (selected == copyValueAction) {
        emit copyValueRequested(index);
    } else if (selected == copyPathAction) {
        emit copyPathRequested(index);
    } else if (selected == copySubtreeAction) {
        emit copySubtreeRequested(index);
    }
}
