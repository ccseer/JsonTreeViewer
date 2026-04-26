#pragma once

#include <QTreeView>

#include "strategies/jsonstrategy.h"

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

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

signals:
    void copyKeyRequested(const QModelIndex& index);
    void copyValueRequested(const QModelIndex& index);
    void copyPathRequested(const QModelIndex& index);
    void copyKeyValueRequested(const QModelIndex& index);
    void copySubtreeRequested(const QModelIndex& index);

private:
    bool m_firstResize = true;
    CopyActions m_copyActions{JsonViewerStrategy::CopyAction::Key
                              | JsonViewerStrategy::CopyAction::Value
                              | JsonViewerStrategy::CopyAction::Path
                              | JsonViewerStrategy::CopyAction::KeyValue
                              | JsonViewerStrategy::CopyAction::Subtree};
};
