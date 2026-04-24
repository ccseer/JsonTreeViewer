#pragma once

#include <QTreeView>

class JsonTreeView : public QTreeView {
    Q_OBJECT
public:
    JsonTreeView(QWidget *parent = nullptr);

    void upadteDPR(qreal);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

signals:
    void copyKeyRequested(const QModelIndex& index);
    void copyValueRequested(const QModelIndex& index);
    void copyPathRequested(const QModelIndex& index);
    void copySubtreeRequested(const QModelIndex& index);
};
