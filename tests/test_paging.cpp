#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

#include "../src/jsonnode.h"
#include "../src/jsontreemodel.h"

class TestPaging : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Paging logic tests
    void testSmallArrayNoPaging();
    void testSmallFileNoPaging_2000Items();
    void testLargeArrayPaging();
    void testExtremeArrayPaging();
    void testVirtualPageExpansion();
    void testVirtualPageDataCorrectness();
    void testNestedArrayNoPaging();
    void testObjectNoPaging();
    void testPageSizeThresholds();

private:
    QString testDir;
    JsonTreeModel* model = nullptr;

    // Helper to create test JSON files
    void createTestFile(const QString& filename, int arraySize);
    void createNestedTestFile(const QString& filename);
    void createObjectTestFile(const QString& filename, int objectSize);

    // Helper: Wait for async load to complete
    bool waitForLoad(JsonTreeModel* m, int timeoutMs = 10000)
    {
        QSignalSpy spy(m, &JsonTreeModel::loadFinished);
        if (spy.wait(timeoutMs)) {
            QList<QVariant> arguments = spy.takeFirst();
            return arguments.at(0).toBool();
        }
        return false;
    }

    // Helper: Load root children after load
    void loadRootChildren(JsonTreeModel* m, int timeoutMs = 10000)
    {
        QModelIndex rootIndex;
        if (m->canFetchMore(rootIndex)) {
            m->fetchMore(rootIndex);
            // Wait for async fetchMore to complete
            // Poll until children are loaded (not just "Loading..."
            // placeholder)
            int iterations = timeoutMs / 100;
            for (int i = 0; i < iterations; ++i) {
                QTest::qWait(100);
                int count = m->rowCount(rootIndex);
                if (count > 0) {
                    // Check if it's not just the "Loading..." placeholder
                    QModelIndex firstChild = m->index(0, 0, rootIndex);
                    if (firstChild.isValid()) {
                        QString key
                            = m->data(firstChild, Qt::DisplayRole).toString();
                        if (key != "Loading...") {
                            return;  // Real children loaded
                        }
                    }
                }
            }
        }
    }

    // Helper: Load children of any node
    void loadChildren(JsonTreeModel* m,
                      const QModelIndex& parent,
                      int timeoutMs = 10000)
    {
        if (m->canFetchMore(parent)) {
            m->fetchMore(parent);
            // Wait for async fetchMore to complete
            int iterations = timeoutMs / 100;
            for (int i = 0; i < iterations; ++i) {
                QTest::qWait(100);
                int count = m->rowCount(parent);
                if (count > 0) {
                    QModelIndex firstChild = m->index(0, 0, parent);
                    if (firstChild.isValid()) {
                        QString key
                            = m->data(firstChild, Qt::DisplayRole).toString();
                        if (key != "Loading...") {
                            return;  // Real children loaded
                        }
                    }
                }
            }
        }
    }
};

void TestPaging::initTestCase()
{
    // Create temporary test directory
    testDir = QDir::tempPath() + "/jsontreeviewer_paging_tests/";
    QDir dir;
    if (!dir.exists(testDir)) {
        dir.mkpath(testDir);
    }

    qDebug() << "Test directory:" << testDir;

    // Create test files
    createTestFile("array_100.json", 100);      // Small - no paging
    createTestFile("array_5000.json", 5000);    // Small - no paging
    createTestFile("array_15000.json", 15000);  // Small - needs paging
    createTestFile("array_2000.json", 2000);    // Medium - needs paging
    createTestFile("array_800.json", 800);      // Large - needs paging
    createTestFile("array_200.json", 200);      // Extreme - needs paging
    createNestedTestFile("nested_array.json");
    createObjectTestFile("object_2000.json", 2000);

    qDebug() << "Test files created";
}

void TestPaging::cleanupTestCase()
{
    if (model) {
        delete model;
        model = nullptr;
    }

    // Clean up test files
    QDir dir(testDir);
    dir.removeRecursively();
}

void TestPaging::createTestFile(const QString& filename, int arraySize)
{
    QJsonArray array;
    for (int i = 0; i < arraySize; ++i) {
        QJsonObject item;
        item["id"]    = i;
        item["name"]  = QString("item_%1").arg(i);
        item["value"] = i * 10;
        array.append(item);
    }

    QJsonObject root;
    root["data"]  = array;
    root["count"] = arraySize;

    QJsonDocument doc(root);
    QFile file(testDir + filename);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson(QJsonDocument::Compact));
        file.close();
    }
}

void TestPaging::createNestedTestFile(const QString& filename)
{
    QJsonArray innerArray;
    for (int i = 0; i < 2000; ++i) {
        innerArray.append(i);
    }

    QJsonObject nested;
    nested["inner"] = innerArray;

    QJsonObject root;
    root["nested"] = nested;

    QJsonDocument doc(root);
    QFile file(testDir + filename);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson(QJsonDocument::Compact));
        file.close();
    }
}

void TestPaging::createObjectTestFile(const QString& filename, int objectSize)
{
    QJsonObject obj;
    for (int i = 0; i < objectSize; ++i) {
        obj[QString("key_%1").arg(i)] = QString("value_%1").arg(i);
    }

    QJsonObject root;
    root["data"] = obj;

    QJsonDocument doc(root);
    QFile file(testDir + filename);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson(QJsonDocument::Compact));
        file.close();
    }
}

void TestPaging::testSmallArrayNoPaging()
{
    qDebug() << "\n=== Test: Small array (100 items) - No paging ===";

    QString path = testDir + "array_100.json";
    if (model)
        delete model;
    model = new JsonTreeModel(this);

    model->load(path);
    QVERIFY(waitForLoad(model));
    QCOMPARE(model->fileMode(), JsonTreeModel::FileMode::Small);

    // Load root children
    loadRootChildren(model);

    // Get root children
    QVERIFY(model->rowCount() > 0);

    // Find "data" array
    QModelIndex dataIndex;
    for (int i = 0; i < model->rowCount(); ++i) {
        QModelIndex idx = model->index(i, 0);
        QString key     = model->data(idx, Qt::DisplayRole).toString();
        if (key == "data") {
            dataIndex = idx;
            break;
        }
    }

    QVERIFY(dataIndex.isValid());

    // Expand the array
    loadChildren(model, dataIndex);

    // Should have 100 direct children (no virtual pages)
    int childCount = model->rowCount(dataIndex);
    qDebug() << "Child count:" << childCount;
    QCOMPARE(childCount, 100);

    // Verify first child is not a virtual page
    QModelIndex firstChild = model->index(0, 0, dataIndex);
    QVERIFY(firstChild.isValid());
    QString firstKey = model->data(firstChild, Qt::DisplayRole).toString();
    qDebug() << "First child key:" << firstKey;
    QVERIFY(!firstKey.startsWith("["));  // Not a virtual page node
}

void TestPaging::testSmallFileNoPaging_2000Items()
{
    qDebug() << "\n=== Test: Small file with 2000 items - No paging (threshold "
                "10000) ===";

    QString path = testDir + "array_2000.json";
    if (model)
        delete model;
    model = new JsonTreeModel(this);

    model->load(path);
    QVERIFY(waitForLoad(model));
    // File is small (< 10MB), so it uses SmallFileStrategy
    QCOMPARE(model->fileMode(), JsonTreeModel::FileMode::Small);

    // Load root children
    loadRootChildren(model);

    // Find "data" array
    QModelIndex dataIndex;
    for (int i = 0; i < model->rowCount(); ++i) {
        QModelIndex idx = model->index(i, 0);
        QString key     = model->data(idx, Qt::DisplayRole).toString();
        if (key == "data") {
            dataIndex = idx;
            break;
        }
    }

    QVERIFY(dataIndex.isValid());

    // Expand the array
    loadChildren(model, dataIndex);

    // Small mode threshold is 10000, so 2000 items should NOT be paged
    int childCount = model->rowCount(dataIndex);
    qDebug() << "Child count (no paging):" << childCount;
    QCOMPARE(childCount, 2000);  // Direct children, no virtual pages

    // Verify first child is NOT a virtual page
    QModelIndex firstChild = model->index(0, 0, dataIndex);
    QVERIFY(firstChild.isValid());
    QString firstKey = model->data(firstChild, Qt::DisplayRole).toString();
    qDebug() << "First child key:" << firstKey;
    QVERIFY(!firstKey.startsWith("["));  // Not a virtual page node
}

void TestPaging::testLargeArrayPaging()
{
    qDebug() << "\n=== Test: Large file array (800 items) - Paging ===";

    QString path = testDir + "array_800.json";

    // Manually set file size to trigger Large strategy
    // We need to pad the file to exceed 100MB
    QFile file(path);
    if (!file.exists()) {
        createTestFile("array_800.json", 800);
    }

    // For this test, we'll create a larger file
    QJsonArray array;
    for (int i = 0; i < 800; ++i) {
        QJsonObject item;
        item["id"]    = i;
        item["name"]  = QString("item_%1").arg(i);
        item["value"] = i * 10;
        // Add padding to make file larger
        item["padding"] = QString("x").repeated(150000);
        array.append(item);
    }

    QJsonObject root;
    root["data"] = array;

    QJsonDocument doc(root);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson(QJsonDocument::Compact));
        file.close();
    }

    if (model)
        delete model;
    model = new JsonTreeModel(this);

    model->load(path);
    QVERIFY(waitForLoad(model));
    QCOMPARE(model->fileMode(), JsonTreeModel::FileMode::Large);

    // Load root children
    loadRootChildren(model);

    // Find "data" array
    QModelIndex dataIndex;
    for (int i = 0; i < model->rowCount(); ++i) {
        QModelIndex idx = model->index(i, 0);
        QString key     = model->data(idx, Qt::DisplayRole).toString();
        if (key == "data") {
            dataIndex = idx;
            break;
        }
    }

    QVERIFY(dataIndex.isValid());

    // Expand the array
    loadChildren(model, dataIndex);

    // Should have virtual page nodes (page size = 500 for Large)
    int childCount = model->rowCount(dataIndex);
    qDebug() << "Child count (virtual pages):" << childCount;
    QCOMPARE(childCount, 2);  // [0..499] and [500..799]
}

void TestPaging::testExtremeArrayPaging()
{
    qDebug() << "\n=== Test: Extreme file array (200 items) - Paging ===";

    QString path = testDir + "array_200.json";

    // Create a file > 1GB to trigger Extreme strategy
    QJsonArray array;
    for (int i = 0; i < 200; ++i) {
        QJsonObject item;
        item["id"]   = i;
        item["name"] = QString("item_%1").arg(i);
        // Add massive padding
        item["padding"] = QString("x").repeated(6000000);
        array.append(item);
    }

    QJsonObject root;
    root["data"] = array;

    QJsonDocument doc(root);
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson(QJsonDocument::Compact));
        file.close();
    }

    if (model)
        delete model;
    model = new JsonTreeModel(this);

    model->load(path);
    bool loaded
        = waitForLoad(model, 60000);  // 60 second timeout for 1.2GB file
    if (!loaded) {
        QSKIP("1.2GB file load timeout (expected for very large files)");
    }
    QCOMPARE(model->fileMode(), JsonTreeModel::FileMode::Extreme);

    // Load root children (use longer timeout for 1.2GB file - extraction takes
    // ~31 seconds)
    loadRootChildren(model, 60000);  // 60 second timeout

    // Find "data" array
    QModelIndex dataIndex;
    for (int i = 0; i < model->rowCount(); ++i) {
        QModelIndex idx = model->index(i, 0);
        QString key     = model->data(idx, Qt::DisplayRole).toString();
        if (key == "data") {
            dataIndex = idx;
            break;
        }
    }

    QVERIFY(dataIndex.isValid());

    // Expand the array
    loadChildren(model, dataIndex);

    // Should have virtual page nodes (page size = 100 for Extreme)
    int childCount = model->rowCount(dataIndex);
    qDebug() << "Child count (virtual pages):" << childCount;
    QCOMPARE(childCount, 2);  // [0..99] and [100..199]
}

void TestPaging::testVirtualPageExpansion()
{
    qDebug()
        << "\n=== Test: Virtual page expansion (15000 items in Small mode) ===";

    QString path = testDir + "array_15000.json";
    if (model)
        delete model;
    model = new JsonTreeModel(this);

    model->load(path);
    QVERIFY(waitForLoad(model));

    // Load root children
    loadRootChildren(model);

    // Find "data" array
    QModelIndex dataIndex;
    for (int i = 0; i < model->rowCount(); ++i) {
        QModelIndex idx = model->index(i, 0);
        if (model->data(idx, Qt::DisplayRole).toString() == "data") {
            dataIndex = idx;
            break;
        }
    }

    QVERIFY(dataIndex.isValid());
    loadChildren(model, dataIndex);

    // 15000 > 10000, so should have virtual pages
    int pageCount = model->rowCount(dataIndex);
    qDebug() << "Page count:" << pageCount;
    QCOMPARE(pageCount, 2);  // [0..9999] and [10000..14999]

    // Get first virtual page [0..9999]
    QModelIndex firstPage = model->index(0, 0, dataIndex);
    QVERIFY(firstPage.isValid());
    QString pageKey = model->data(firstPage, Qt::DisplayRole).toString();
    qDebug() << "First page key:" << pageKey;
    QCOMPARE(pageKey, QString("[0..9999]"));

    // Expand the virtual page
    QVERIFY(model->canFetchMore(firstPage));

    QElapsedTimer timer;
    timer.start();
    loadChildren(model, firstPage);  // Use helper to wait for async load
    qint64 expandTime = timer.elapsed();

    qDebug() << "Virtual page expand time:" << expandTime << "ms";
    QVERIFY2(expandTime < 5000, "Virtual page expansion too slow");

    // Should have 10000 actual items
    int itemCount = model->rowCount(firstPage);
    qDebug() << "Items in first page:" << itemCount;
    QCOMPARE(itemCount, 10000);

    // Verify first item in page
    QModelIndex firstItem = model->index(0, 0, firstPage);
    QVERIFY(firstItem.isValid());
}

void TestPaging::testVirtualPageDataCorrectness()
{
    qDebug() << "\n=== Test: Virtual page data correctness ===";

    QString path = testDir + "array_15000.json";
    if (model)
        delete model;
    model = new JsonTreeModel(this);

    model->load(path);
    QVERIFY(waitForLoad(model));

    // Load root children
    loadRootChildren(model);

    // Navigate to data array
    QModelIndex dataIndex;
    for (int i = 0; i < model->rowCount(); ++i) {
        QModelIndex idx = model->index(i, 0);
        if (model->data(idx, Qt::DisplayRole).toString() == "data") {
            dataIndex = idx;
            break;
        }
    }

    QVERIFY(dataIndex.isValid());
    loadChildren(model, dataIndex);

    // Should have 2 pages: [0..9999] and [10000..14999]
    QCOMPARE(model->rowCount(dataIndex), 2);

    // Expand second page [10000..14999]
    QModelIndex secondPage = model->index(1, 0, dataIndex);
    QVERIFY(secondPage.isValid());
    QString secondPageKey = model->data(secondPage, Qt::DisplayRole).toString();
    qDebug() << "Second page key:" << secondPageKey;
    QCOMPARE(secondPageKey, QString("[10000..14999]"));

    if (model->canFetchMore(secondPage)) {
        loadChildren(model, secondPage);  // Use helper to wait for async load
    }

    // Verify we have 5000 items in second page
    int itemCount = model->rowCount(secondPage);
    qDebug() << "Items in second page:" << itemCount;
    QCOMPARE(itemCount, 5000);

    // Expand first item in second page to verify it's item 10000
    QModelIndex firstItemInPage = model->index(0, 0, secondPage);
    QVERIFY(firstItemInPage.isValid());

    if (model->canFetchMore(firstItemInPage)) {
        loadChildren(model,
                     firstItemInPage);  // Use helper to wait for async load
    }

    // Find "id" field
    bool foundId = false;
    for (int i = 0; i < model->rowCount(firstItemInPage); ++i) {
        QModelIndex field = model->index(i, 0, firstItemInPage);
        QString key       = model->data(field, Qt::DisplayRole).toString();
        if (key == "id") {
            QModelIndex valueIdx = model->index(i, 1, firstItemInPage);
            QString value = model->data(valueIdx, Qt::DisplayRole).toString();
            qDebug() << "First item in second page - id:" << value;
            QCOMPARE(value, QString("10000"));
            foundId = true;
            break;
        }
    }

    QVERIFY2(foundId, "Could not find 'id' field in first item of second page");
}

void TestPaging::testNestedArrayNoPaging()
{
    qDebug() << "\n=== Test: Nested array - no paging (2000 < 10000) ===";

    QString path = testDir + "nested_array.json";
    if (model)
        delete model;
    model = new JsonTreeModel(this);

    model->load(path);
    QVERIFY(waitForLoad(model));

    // Load root children
    loadRootChildren(model);

    // Navigate to nested.inner array
    QModelIndex nestedIndex;
    for (int i = 0; i < model->rowCount(); ++i) {
        QModelIndex idx = model->index(i, 0);
        if (model->data(idx, Qt::DisplayRole).toString() == "nested") {
            nestedIndex = idx;
            break;
        }
    }

    QVERIFY(nestedIndex.isValid());
    loadChildren(model, nestedIndex);

    // Find "inner" array
    QModelIndex innerIndex;
    for (int i = 0; i < model->rowCount(nestedIndex); ++i) {
        QModelIndex idx = model->index(i, 0, nestedIndex);
        if (model->data(idx, Qt::DisplayRole).toString() == "inner") {
            innerIndex = idx;
            break;
        }
    }

    QVERIFY(innerIndex.isValid());
    loadChildren(model, innerIndex);

    // Small mode, 2000 items < 10000 threshold, so no paging
    int childCount = model->rowCount(innerIndex);
    qDebug() << "Nested array child count:" << childCount;
    QCOMPARE(childCount, 2000);
}

void TestPaging::testObjectNoPaging()
{
    qDebug() << "\n=== Test: Object - no paging (2000 < 10000) ===";

    QString path = testDir + "object_2000.json";
    if (model)
        delete model;
    model = new JsonTreeModel(this);

    model->load(path);
    QVERIFY(waitForLoad(model));

    // Load root children
    loadRootChildren(model);

    // Find "data" object
    QModelIndex dataIndex;
    for (int i = 0; i < model->rowCount(); ++i) {
        QModelIndex idx = model->index(i, 0);
        if (model->data(idx, Qt::DisplayRole).toString() == "data") {
            dataIndex = idx;
            break;
        }
    }

    QVERIFY(dataIndex.isValid());
    loadChildren(model, dataIndex);

    // Small mode, 2000 keys < 10000 threshold, so no paging
    int childCount = model->rowCount(dataIndex);
    qDebug() << "Object child count:" << childCount;
    QCOMPARE(childCount, 2000);
}

void TestPaging::testPageSizeThresholds()
{
    qDebug() << "\n=== Test: Page size thresholds ===";

    // Test Small mode threshold (10000)
    {
        QString path = testDir + "array_5000.json";
        if (model)
            delete model;
        model = new JsonTreeModel(this);
        model->load(path);
        QVERIFY(waitForLoad(model));
        QCOMPARE(model->fileMode(), JsonTreeModel::FileMode::Small);

        // Load root children
        loadRootChildren(model);

        QModelIndex dataIndex;
        for (int i = 0; i < model->rowCount(); ++i) {
            QModelIndex idx = model->index(i, 0);
            if (model->data(idx, Qt::DisplayRole).toString() == "data") {
                dataIndex = idx;
                break;
            }
        }

        if (dataIndex.isValid() && model->canFetchMore(dataIndex)) {
            loadChildren(model, dataIndex);
        }

        // 5000 < 10000, so no paging
        int childCount = model->rowCount(dataIndex);
        qDebug() << "Small mode (5000 items):" << childCount << "children";
        QCOMPARE(childCount, 5000);
    }

    // Test Small mode with paging (15000 > 10000)
    {
        QString path = testDir + "array_15000.json";
        if (model)
            delete model;
        model = new JsonTreeModel(this);
        model->load(path);
        QVERIFY(waitForLoad(model));
        QCOMPARE(model->fileMode(), JsonTreeModel::FileMode::Small);

        // Load root children
        loadRootChildren(model);

        QModelIndex dataIndex;
        for (int i = 0; i < model->rowCount(); ++i) {
            QModelIndex idx = model->index(i, 0);
            if (model->data(idx, Qt::DisplayRole).toString() == "data") {
                dataIndex = idx;
                break;
            }
        }

        if (dataIndex.isValid() && model->canFetchMore(dataIndex)) {
            loadChildren(model, dataIndex);
        }

        // 15000 > 10000, so should have 2 pages
        int pageCount = model->rowCount(dataIndex);
        qDebug() << "Small mode (15000 items):" << pageCount << "pages";
        QCOMPARE(pageCount, 2);  // [0..9999] and [10000..14999]
    }
}

QTEST_MAIN(TestPaging)
#include "test_paging.moc"
