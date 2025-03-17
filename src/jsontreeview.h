#pragma once

#include <QTreeView>

class JsonTreeView : public QTreeView {
    Q_OBJECT
public:
    JsonTreeView(QWidget *parent = nullptr);

    void upadteDPR(qreal);
};
