#pragma once

#include <QModelIndex>
#include <QObject>
#include <QPointer>
#include <QStringList>

class JsonTreeModel;
class JsonTreeItem;

enum class NavigationError {
    Success,
    InvalidPointer,
    PathNotFound,
    NotAContainer,
    FetchTimeout
};

class QTimer;

class PathNavigator : public QObject {
    Q_OBJECT
public:
    explicit PathNavigator(QObject* parent = nullptr);

    void navigate(JsonTreeModel* model, const QString& jsonPointer);

    QModelIndex currentIndex() const
    {
        return m_currentIndex;
    }

signals:
    void navigationCompleted(NavigationError error, const QString& message);
    void navigationProgress(int currentDepth, int totalDepth);

private slots:
    void onFetchQueueChanged(int queueSize, bool inProgress);
    void onTimeout();

private:
    void navigateNextLevel();
    QString unescapeSegment(const QString& segment);
    JsonTreeItem* findChild(JsonTreeItem* parentItem, const QString& segment);
    JsonTreeItem* findInPagedArray(JsonTreeItem* arrayNode, int targetIndex);

    QPointer<JsonTreeModel> m_model;
    QStringList m_pathSegments;
    int m_currentDepth = 0;
    QModelIndex m_currentIndex;
    bool m_waitingForFetch = false;
    QTimer* m_timeoutTimer;
};
