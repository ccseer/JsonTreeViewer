#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

#include "../src/jsontreemodel.h"
#include "../src/jsontreeview.h"
#include "../src/navigation/pathnavigator.h"

class TestNavigation : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testSimplePath();
    void testEscapedPath();
    void testPagingPath();

private:
    QString testDir;
    JsonTreeModel* model = nullptr;
    JsonTreeView* view   = nullptr;

    bool waitForLoad(JsonTreeModel* m, int timeoutMs = 5000)
    {
        QSignalSpy spy(m, &JsonTreeModel::loadFinished);
        return spy.wait(timeoutMs);
    }
};

void TestNavigation::initTestCase()
{
    testDir = QDir::tempPath() + "/jsontreeviewer_nav_tests/";
    QDir().mkpath(testDir);

    // Create test file with paging
    QJsonArray array;
    for (int i = 0; i < 2000; ++i) {
        QJsonObject obj;
        obj["id"]             = i;
        obj["key/with/slash"] = "slash";
        obj["key~with~tilde"] = "tilde";
        array.append(obj);
    }
    QJsonObject root;
    root["items"] = array;

    QFile file(testDir + "nav_test.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        file.close();
    }
}

void TestNavigation::cleanupTestCase()
{
    QDir(testDir).removeRecursively();
}

void TestNavigation::testSimplePath()
{
    model = new JsonTreeModel(this);
    view  = new JsonTreeView();
    view->setModel(model);

    model->load(testDir + "nav_test.json");
    QVERIFY(waitForLoad(model));

    PathNavigator nav;
    QSignalSpy spy(&nav, &PathNavigator::navigationCompleted);

    nav.navigate(model, "/items/0/id");

    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.at(0).at(0).toInt(),
             static_cast<int>(NavigationError::Success));
}

void TestNavigation::testEscapedPath()
{
    model = new JsonTreeModel(this);
    view  = new JsonTreeView();
    view->setModel(model);

    model->load(testDir + "nav_test.json");
    QVERIFY(waitForLoad(model));

    PathNavigator nav;
    QSignalSpy spy(&nav, &PathNavigator::navigationCompleted);

    // Testing "~1" for "/" and "~0" for "~"
    nav.navigate(model, "/items/0/key~1with~1slash");

    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.at(0).at(0).toInt(),
             static_cast<int>(NavigationError::Success));
}

void TestNavigation::testPagingPath()
{
    model = new JsonTreeModel(this);
    view  = new JsonTreeView();
    view->setModel(model);

    model->load(testDir + "nav_test.json");
    QVERIFY(waitForLoad(model));

    PathNavigator nav;
    QSignalSpy spy(&nav, &PathNavigator::navigationCompleted);

    nav.navigate(model, "/items/1500/id");

    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.at(0).at(0).toInt(),
             static_cast<int>(NavigationError::Success));

    // Verify resolved index
    QModelIndex resolved = nav.currentIndex();
    QVERIFY(resolved.isValid());
    QCOMPARE(model->data(resolved, Qt::DisplayRole).toString(), QString("id"));
}

QTEST_MAIN(TestNavigation)
#include "test_navigation.moc"
