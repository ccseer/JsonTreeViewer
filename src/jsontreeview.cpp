#include "jsontreeview.h"

#include <QContextMenuEvent>
#include <QHeaderView>
#include <QMenu>
#include <QTimer>

JsonTreeView::JsonTreeView(QWidget* parent) : QTreeView(parent)
{
    setUniformRowHeights(true);
    setAnimated(false);
    setContextMenuPolicy(Qt::DefaultContextMenu);
    header()->setStretchLastSection(true);
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
    if (!index.isValid())
        return;

    using CA = JsonViewerStrategy::CopyAction;

    QMenu menu(this);

    QAction* copyKeyAction      = nullptr;
    QAction* copyValueAction    = nullptr;
    QAction* copyPathAction     = nullptr;
    QAction* copyDotPathAction  = nullptr;
    QAction* copyKeyValueAction = nullptr;
    QAction* copySubtreeAction  = nullptr;
    QAction* exportAction       = nullptr;

    if (m_copyActions & CA::Key)
        copyKeyAction = menu.addAction(tr("Copy Key"));
    if (m_copyActions & CA::Value)
        copyValueAction = menu.addAction(tr("Copy Value"));
    if (m_copyActions & CA::Path)
        copyPathAction = menu.addAction(tr("Copy Path (JSON Pointer)"));

    // Always show Dot Path (JavaScript style)
    copyDotPathAction = menu.addAction(tr("Copy Dot Path"));

    if (m_copyActions & CA::KeyValue)
        copyKeyValueAction = menu.addAction(tr("Copy Key:Value"));

    menu.addSeparator();

    if (m_copyActions & CA::Subtree)
        copySubtreeAction = menu.addAction(tr("Copy Subtree"));

    // Always show Export
    exportAction = menu.addAction(tr("Export Selection to File"));

    QAction* selected = menu.exec(event->globalPos());
    if (!selected)
        return;

    if (selected == copyKeyAction)
        emit copyKeyRequested(index);
    else if (selected == copyValueAction)
        emit copyValueRequested(index);
    else if (selected == copyPathAction)
        emit copyPathRequested(index);
    else if (selected == copyDotPathAction)
        emit copyDotPathRequested(index);
    else if (selected == copyKeyValueAction)
        emit copyKeyValueRequested(index);
    else if (selected == copySubtreeAction)
        emit copySubtreeRequested(index);
    else if (selected == exportAction)
        emit exportSelectionRequested(index);
}

void JsonTreeView::resizeEvent(QResizeEvent* event)
{
    QTreeView::resizeEvent(event);

    if (m_firstResize && width() > 40) {
        header()->resizeSection(0, width() / 3);
        m_firstResize = false;
    }
}
