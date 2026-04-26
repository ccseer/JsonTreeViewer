#include "jsontreeview.h"

#include <QContextMenuEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QHeaderView>
#include <QMenu>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QUrl>

#include "jsonnode.h"
#include "jsontreemodel.h"

namespace {
// Check if a string value is a URL or email
bool isUrl(const QString& value)
{
    return value.startsWith("http://", Qt::CaseInsensitive)
           || value.startsWith("https://", Qt::CaseInsensitive)
           || value.startsWith("ftp://", Qt::CaseInsensitive)
           || value.startsWith("ftps://", Qt::CaseInsensitive)
           || value.startsWith("mailto:", Qt::CaseInsensitive)
           || (value.contains('@') && value.contains('.')
               && value.indexOf('@') > 0
               && value.indexOf('@') < value.lastIndexOf('.'));
}

// Check if a string value is a Unix timestamp
// Returns: 0 = not a timestamp, 10 = seconds, 13 = milliseconds
int isTimestamp(const QString& value)
{
    bool ok;
    qint64 num = value.toLongLong(&ok);
    if (!ok)
        return 0;

    // Check for 10-digit timestamp (seconds since epoch)
    if (value.length() == 10) {
        // Range check: 1970-01-01 to 2100-01-01
        if (num >= 0 && num <= 4102444800LL)
            return 10;
    }
    // Check for 13-digit timestamp (milliseconds since epoch)
    else if (value.length() == 13) {
        // Range check: 1970-01-01 to 2100-01-01
        if (num >= 0 && num <= 4102444800000LL)
            return 13;
    }

    return 0;
}

// Format timestamp as human-readable string
QString formatTimestamp(const QString& value, int type)
{
    qint64 num = value.toLongLong();
    QDateTime dt;

    if (type == 10) {
        // Seconds
        dt = QDateTime::fromSecsSinceEpoch(num, Qt::UTC);
    }
    else if (type == 13) {
        // Milliseconds
        dt = QDateTime::fromMSecsSinceEpoch(num, Qt::UTC);
    }
    else {
        return QString();
    }

    if (type == 13) {
        return dt.toString("yyyy-MM-dd HH:mm:ss.zzz") + " UTC";
    }
    else {
        return dt.toString("yyyy-MM-dd HH:mm:ss") + " UTC";
    }
}

// Format timestamp as ISO 8601
QString formatTimestampIso8601(const QString& value, int type)
{
    qint64 num = value.toLongLong();
    QDateTime dt;

    if (type == 10) {
        dt = QDateTime::fromSecsSinceEpoch(num, Qt::UTC);
    }
    else if (type == 13) {
        dt = QDateTime::fromMSecsSinceEpoch(num, Qt::UTC);
    }
    else {
        return QString();
    }

    return dt.toString(Qt::ISODate);
}
}  // namespace

JsonTreeView::JsonTreeView(QWidget* parent) : QTreeView(parent)
{
    setUniformRowHeights(true);
    setAnimated(false);
    setContextMenuPolicy(Qt::DefaultContextMenu);
    header()->setStretchLastSection(true);

    // Add global shortcuts for collapse/expand all
    QAction* collapseAction = new QAction(this);
    collapseAction->setShortcut(QKeySequence("Ctrl+Shift+["));
    connect(collapseAction, &QAction::triggered, this,
            &JsonTreeView::collapseAllRequested);
    addAction(collapseAction);

    QAction* expandAction = new QAction(this);
    expandAction->setShortcut(QKeySequence("Ctrl+Shift+]"));
    connect(expandAction, &QAction::triggered, this, [this]() {
        if (m_file_mode == FileMode::Small)
            emit expandAllRequested();
    });
    addAction(expandAction);
}

void JsonTreeView::setModel(QAbstractItemModel* model)
{
    QTreeView::setModel(model);
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

    // Check if this is an error tree by looking at the root node type
    // Error trees have a root with type 'r' and first child with type 'E'
    auto* treeModel = qobject_cast<JsonTreeModel*>(
        qobject_cast<TreeFilterProxyModel*>(model())->sourceModel());
    if (treeModel) {
        auto* rootItem = treeModel->getItem(QModelIndex());
        if (rootItem && !rootItem->children.isEmpty()) {
            auto* firstChild = rootItem->children.first();
            if (firstChild && firstChild->type == 'E') {
                // This is an error tree, disable context menu
                return;
            }
        }
    }

    using CA = JsonViewerStrategy::CopyAction;

    QMenu menu(this);

    // Actions for valid items only
    if (index.isValid()) {
        // Get value to check for URL or timestamp
        QString value     = index.sibling(index.row(), 1).data().toString();
        bool hasUrl       = isUrl(value);
        int timestampType = isTimestamp(value);
        // Check if this is a container (object/array) by checking if it has
        // children
        bool isContainer = model()->hasChildren(index);

        if (m_copyActions & CA::Key)
            menu.addAction(tr("Copy Key"), this,
                           [this, index]() { emit copyKeyRequested(index); });
        if (m_copyActions & CA::Value) {
            QAction* copyValueAction = menu.addAction(
                tr("Copy Value"), this,
                [this, index]() { emit copyValueRequested(index); });
            copyValueAction->setEnabled(!isContainer);
        }
        if (m_copyActions & CA::Path)
            menu.addAction(tr("Copy Path (JSON Pointer)"), this,
                           [this, index]() { emit copyPathRequested(index); });

        // Always show Dot Path (JavaScript style)
        menu.addAction(tr("Copy Dot Path"), this,
                       [this, index]() { emit copyDotPathRequested(index); });

        if (m_copyActions & CA::KeyValue)
            menu.addAction(tr("Copy Key:Value"), this, [this, index]() {
                emit copyKeyValueRequested(index);
            });

        // Special actions for URL values
        if (hasUrl) {
            menu.addSeparator();
            menu.addAction(tr("Open URL in Browser"), this,
                           [this, value]() { emit openUrlRequested(value); });
        }

        // Special actions for timestamp values
        if (timestampType > 0) {
            menu.addSeparator();
            QString readableTime = formatTimestamp(value, timestampType);
            QAction* timestampAction
                = menu.addAction(tr("Copy as ISO 8601"), this, [this, value]() {
                      emit copyTimestampAsIso8601Requested(value);
                  });
            timestampAction->setToolTip(readableTime);
        }

        menu.addSeparator();

        if (m_copyActions & CA::Subtree)
            menu.addAction(tr("Copy Subtree"), this, [this, index]() {
                emit copySubtreeRequested(index);
            });

        // Always show Export
        menu.addAction(tr("Export Selection to File"), this, [this, index]() {
            emit exportSelectionRequested(index);
        });

        menu.addSeparator();
    }

    // Common Tree Control Actions (shown for both empty space and items)
    QAction* collapseAllAction = menu.addAction(tr("Collapse All"));
    collapseAllAction->setShortcut(QKeySequence("Ctrl+Shift+["));
    connect(collapseAllAction, &QAction::triggered, this,
            &JsonTreeView::collapseAllRequested);

    // Only show Expand All for Small files
    if (m_file_mode == FileMode::Small) {
        QAction* expandAllAction = menu.addAction(tr("Expand All"));
        expandAllAction->setShortcut(QKeySequence("Ctrl+Shift+]"));
        connect(expandAllAction, &QAction::triggered, this,
                &JsonTreeView::expandAllRequested);
    }

    menu.exec(event->globalPos());
}

void JsonTreeView::resizeEvent(QResizeEvent* event)
{
    QTreeView::resizeEvent(event);

    if (m_firstResize && width() > 40) {
        header()->resizeSection(0, width() / 3);
        m_firstResize = false;
    }
}
