#pragma once

#include <QTreeView>

#include "common.h"
#include "strategies/jsonstrategy.h"

// Forward declaration
class JsonTreeModel;

class JsonTreeView : public QTreeView {
    Q_OBJECT
public:
    using CopyActions = JsonViewerStrategy::CopyActions;

    JsonTreeView(QWidget* parent = nullptr);

    void upadteDPR(qreal);
    void setCopyActions(CopyActions actions)
    {
        m_copyActions = actions;
    }
    void setFileMode(FileMode mode)
    {
        m_file_mode = mode;
    }
    void setModel(QAbstractItemModel* model) override;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void drawBranches(QPainter* painter,
                      const QRect& rect,
                      const QModelIndex& index) const override;

signals:
    void copyKeyRequested(const QModelIndex& index);
    void copyValueRequested(const QModelIndex& index);
    void copyPathRequested(const QModelIndex& index);
    void copyDotPathRequested(const QModelIndex& index);
    void copyKeyValueRequested(const QModelIndex& index);
    void copySubtreeRequested(const QModelIndex& index);
    void exportSelectionRequested(const QModelIndex& index);
    void collapseAllRequested();
    void expandAllRequested();
    void openUrlRequested(const QString& url);
    void copyTimestampAsIso8601Requested(const QString& value);

private:
    bool m_firstResize   = true;
    FileMode m_file_mode = FileMode::Small;
    CopyActions m_copyActions{JsonViewerStrategy::CopyAction::Key
                              | JsonViewerStrategy::CopyAction::Value
                              | JsonViewerStrategy::CopyAction::Path
                              | JsonViewerStrategy::CopyAction::KeyValue
                              | JsonViewerStrategy::CopyAction::Subtree};
};
