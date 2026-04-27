#include "pathnavigator.h"

#include <QDebug>
#include <QTimer>
#include <cstdio>

#include "../jsonnode.h"
#include "../jsontreemodel.h"

PathNavigator::PathNavigator(QObject* parent) : QObject(parent)
{
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &PathNavigator::onTimeout);
}

void PathNavigator::navigate(JsonTreeModel* model, const QString& jsonPointer)
{
    m_model = model;
    m_timeoutTimer->stop();

    if (!m_model) {
        emit navigationCompleted(NavigationError::InvalidPointer,
                                 tr("No model loaded"));
        return;
    }

    if (!jsonPointer.startsWith("/") && !jsonPointer.isEmpty()) {
        emit navigationCompleted(
            NavigationError::InvalidPointer,
            tr("Invalid JSON pointer: %1").arg(jsonPointer));
        return;
    }

    QString normalized = jsonPointer;
    if (normalized.startsWith("/"))
        normalized.remove(0, 1);

    m_pathSegments    = normalized.split('/', Qt::SkipEmptyParts);
    m_currentDepth    = 0;
    m_currentIndex    = QModelIndex();  // Start at root
    m_waitingForFetch = false;

    disconnect(m_model, &JsonTreeModel::fetchQueueChanged, this,
               &PathNavigator::onFetchQueueChanged);
    connect(m_model, &JsonTreeModel::fetchQueueChanged, this,
            &PathNavigator::onFetchQueueChanged);

    navigateNextLevel();
}

void PathNavigator::navigateNextLevel()
{
    if (m_currentDepth >= m_pathSegments.size()) {
        m_timeoutTimer->stop();
        emit navigationCompleted(NavigationError::Success, "");
        return;
    }

    JsonTreeItem* parentItem = m_model->getItem(m_currentIndex);
    if (!parentItem) {
        m_timeoutTimer->stop();
        emit navigationCompleted(NavigationError::PathNotFound,
                                 tr("Parent item not found"));
        return;
    }

    QString segment = unescapeSegment(m_pathSegments[m_currentDepth]);

    // Check if children are already loaded.
    // JsonTreeItem uses child_count for total children and children.size() for
    // loaded.
    if (parentItem->children.isEmpty()
        && m_model->canFetchMore(m_currentIndex)) {
        m_waitingForFetch = true;
        m_timeoutTimer->start(5000);  // 5 seconds timeout
        m_model->fetchMore(m_currentIndex);
        return;
    }

    JsonTreeItem* child = findChild(parentItem, segment);
    if (child) {
        // Calculate row manually since JsonTreeItem doesn't have row() method
        int row = -1;
        if (parentItem->children.contains(child)) {
            row = parentItem->children.indexOf(child);
        }

        if (row != -1) {
            m_currentIndex = m_model->index(row, 0, m_currentIndex);
            m_currentDepth++;
            emit navigationProgress(m_currentDepth, m_pathSegments.size());
            navigateNextLevel();
        }
        else {
            // This could happen if child is a paged placeholder
            m_timeoutTimer->stop();
            emit navigationCompleted(NavigationError::PathNotFound,
                                     tr("Item found but has no valid row"));
        }
    }
    else {
        m_timeoutTimer->stop();
        emit navigationCompleted(NavigationError::PathNotFound,
                                 tr("Path segment not found: %1").arg(segment));
    }
}

void PathNavigator::onFetchQueueChanged(int queueSize, bool inProgress)
{
    if (m_waitingForFetch && !inProgress && queueSize == 0) {
        m_waitingForFetch = false;
        m_timeoutTimer->stop();
        // Retry navigation at current level
        navigateNextLevel();
    }
}

void PathNavigator::onTimeout()
{
    if (m_waitingForFetch) {
        m_waitingForFetch = false;
        emit navigationCompleted(NavigationError::FetchTimeout,
                                 tr("Timed out waiting for data from model"));
    }
}

JsonTreeItem* PathNavigator::findChild(JsonTreeItem* parentItem,
                                       const QString& segment)
{
    if (parentItem->type == '[') {
        bool ok;
        int index = segment.toInt(&ok);
        if (ok) {
            if (index >= 0 && index < parentItem->children.size()) {
                return parentItem->children[index];
            }
            // Check for paged placeholder
            return findInPagedArray(parentItem, index);
        }
    }
    else {
        for (int i = 0; i < parentItem->children.size(); ++i) {
            if (parentItem->children[i]->key == segment) {
                return parentItem->children[i];
            }
        }
    }
    return nullptr;
}

JsonTreeItem* PathNavigator::findInPagedArray(JsonTreeItem* arrayNode,
                                              int targetIndex)
{
    for (int i = 0; i < arrayNode->children.size(); ++i) {
        JsonTreeItem* child = arrayNode->children[i];
        if (child->key.startsWith("[")) {  // Paged placeholder
            int start, end;
            // Use QByteArray or similar if needed, but here key is QString
            QByteArray keyBytes = child->key.toUtf8();
            if (sscanf(keyBytes.constData(), "[%d..%d]", &start, &end) == 2) {
                if (targetIndex >= start && targetIndex <= end) {
                    return child;
                }
            }
        }
    }
    return nullptr;
}

QString PathNavigator::unescapeSegment(const QString& segment)
{
    QString res = segment;
    res.replace("~1", "/");
    res.replace("~0", "~");
    return res;
}
