#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QString>

#include "jsontreeviewer.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QElapsedTimer et;
    et.start();
    JsonTreeViewer viewer;

    auto p    = std::make_unique<ViewOptions>();
    p->d->dpr = 1;
    // p->d->path     = "D:/1.json";
    p->d->path = "D:/c.json";
    p->d->path = "D:/bug - Copy.json";
    // p->d->path = "C:/d/2.json";
    if (!QFile::exists(p->d->path)) {
        qDebug() << "file not found" << p->d->path;
        return -1;
    }
    p->d->theme = 1;
    p->d->type  = viewer.name();
    viewer.setWindowTitle(p->d->path);
    viewer.load(nullptr, std::move(p));
    qDebug() << "load" << et.restart() << "ms";
    viewer.resize(viewer.getContentSize());
    viewer.show();
    qDebug() << "show" << et.restart() << "ms";

    return app.exec();
}
