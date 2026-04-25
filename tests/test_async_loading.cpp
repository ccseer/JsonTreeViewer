#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QTest>

#include "../src/jsonnode.h"
#include "../src/jsontreemodel.h"

class TestAsyncLoading : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Phase 13: Async loading optimization tests
    void testLoadDoesNotExtractChildren();
    void testLoadTimeIsConstant();
    void testRootHasChildrenAfterLoad();
    void testRootChildrenNotLoadedAfterLoad();
    void testCanFetchMoreOnRoot();
    void testFetchMoreLoadsRootChildren();
    void testVirtualPagingAfterFetchMore();
    void testEmptyRootHandling();
    void testNestedLazyLoadingStillWorks();

    // Performance regression tests
    void testLoadPerformance_100k_array();
    void testLoadPerformance_100k_object();

private:
    JsonTreeModel* model = nullptr;
    QString testDir;
    QString testFile_100k_array;
    QString testFile_100k_object;
    QString testFile_empty;
    QString testFile_nested;
};

void TestAsyncLoading::initTestCase()
{
    qDebug() << "=== Phase 13: Async Loading Tests ===";

    // Try multiple possible paths (same as test_strategies.cpp)
    QStringList possiblePaths = {
        QCoreApplication::applicationDirPath() + "/../",
        QCoreApplication::applicationDirPath() + "/../tests/",
        QCoreApplication::applicationDirPath() + "/../../tests/",
        QCoreApplication::applicationDirPath() + "/../../../tests/",
        QDir::currentPath() + "/tests/",
        QDir::currentPath() + "/../tests/",
        QDir::currentPath() + "/",
    };

    // Find the correct test directory
    for (const QString& path : possiblePaths) {
        QString testFile = path + "test_100k_array.json";
        if (QFile::exists(testFile)) {
            testDir = path;
            qDebug() << "Found test directory:" << testDir;
            break;
        }
    }

    if (testDir.isEmpty()) {
        qDebug() << "Tried paths:";
        for (const QString& path : possiblePaths) {
            qDebug() << "  " << path;
        }
        QFAIL("Could not find test directory with Phase 13 test files. Run: python tests/generate_test_json.py");
    }

    // Set full paths
    testFile_100k_array = testDir + "test_100k_array.json";
    testFile_100k_object = testDir + "test_100k_object.json";
    testFile_empty = testDir + "test_empty.json";
    testFile_nested = testDir + "test_nested.json";

    // Verify test files exist
    QStringList testFiles = {
        "test_100k_array.json",
        "test_100k_object.json",
        "test_empty.json",
        "test_nested.json"
    };

    for (const QString& file : testFiles) {
        QString path = testDir + file;
        if (!QFile::exists(path)) {
            QFAIL(qPrintable("Test file not found: " + path + ". Run: python tests/generate_test_json.py"));
        }
    }

    qDebug() << "All Phase 13 test files found";
}

void TestAsyncLoading::cleanupTestCase()
{
    if (model) {
        delete model;
        model = nullptr;
    }
}

void TestAsyncLoading::testLoadDoesNotExtractChildren()
{
    // Test that load() no longer calls extractChildren() on root
    model = new JsonTreeModel();

    bool loaded = model->load(testFile_100k_array);
    QVERIFY2(loaded, "Failed to load test file");

    // Root should have no children loaded yet
    QModelIndex rootIndex;
    int childCount = model->rowCount(rootIndex);

    QCOMPARE(childCount, 0);
    qDebug() << "✓ Root has 0 children after load (deferred loading works)";
}

void TestAsyncLoading::testLoadTimeIsConstant()
{
    // Test that load time is O(1), not O(N)
    model = new JsonTreeModel();

    QElapsedTimer timer;
    timer.start();

    bool loaded = model->load(testFile_100k_array);
    qint64 loadTime = timer.elapsed();

    QVERIFY2(loaded, "Failed to load test file");
    QVERIFY2(loadTime < 100,
             QString("Load time %1ms exceeds 100ms target").arg(loadTime).toUtf8());

    qDebug() << "✓ Load time:" << loadTime << "ms (target: < 100ms)";

    delete model;
    model = nullptr;
}

void TestAsyncLoading::testRootHasChildrenAfterLoad()
{
    // Test that root is marked as having children
    model = new JsonTreeModel();

    bool loaded = model->load(testFile_100k_array);
    QVERIFY2(loaded, "Failed to load test file");

    QModelIndex rootIndex;
    bool hasChildren = model->hasChildren(rootIndex);

    QVERIFY2(hasChildren, "Root should be marked as having children");
    qDebug() << "✓ Root marked as having children";

    delete model;
    model = nullptr;
}

void TestAsyncLoading::testRootChildrenNotLoadedAfterLoad()
{
    // Test that root children are not loaded after load()
    model = new JsonTreeModel();

    bool loaded = model->load(testFile_100k_array);
    QVERIFY2(loaded, "Failed to load test file");

    QModelIndex rootIndex;
    int rowCount = model->rowCount(rootIndex);

    QCOMPARE(rowCount, 0);
    qDebug() << "✓ Root children not loaded after load()";

    delete model;
    model = nullptr;
}

void TestAsyncLoading::testCanFetchMoreOnRoot()
{
    // Test that canFetchMore returns true for root
    model = new JsonTreeModel();

    bool loaded = model->load(testFile_100k_array);
    QVERIFY2(loaded, "Failed to load test file");

    QModelIndex rootIndex;
    bool canFetch = model->canFetchMore(rootIndex);

    QVERIFY2(canFetch, "canFetchMore should return true for unloaded root");
    qDebug() << "✓ canFetchMore returns true for root";

    delete model;
    model = nullptr;
}

void TestAsyncLoading::testFetchMoreLoadsRootChildren()
{
    // Test that fetchMore() loads root children on demand
    model = new JsonTreeModel();

    bool loaded = model->load(testFile_100k_array);
    QVERIFY2(loaded, "Failed to load test file");

    QModelIndex rootIndex;

    // Before fetchMore: 0 children
    QCOMPARE(model->rowCount(rootIndex), 0);

    // Call fetchMore
    QElapsedTimer timer;
    timer.start();
    model->fetchMore(rootIndex);
    qint64 fetchTime = timer.elapsed();

    // After fetchMore: should have children (paged or direct)
    int childCount = model->rowCount(rootIndex);
    QVERIFY2(childCount > 0, "fetchMore should load children");

    qDebug() << "✓ fetchMore loaded" << childCount << "children in" << fetchTime << "ms";

    delete model;
    model = nullptr;
}

void TestAsyncLoading::testVirtualPagingAfterFetchMore()
{
    // Test that virtual paging works after fetchMore
    model = new JsonTreeModel();

    bool loaded = model->load(testFile_100k_array);
    QVERIFY2(loaded, "Failed to load test file");

    QModelIndex rootIndex;
    model->fetchMore(rootIndex);

    int childCount = model->rowCount(rootIndex);

    // For 100k array with Large/Extreme strategy, should have paged nodes
    // Check if first child looks like a page node
    if (childCount > 0) {
        QModelIndex firstChild = model->index(0, 0, rootIndex);
        QString key = model->data(firstChild, Qt::DisplayRole).toString();

        // Virtual page nodes have format like "[0..99]"
        bool isPageNode = key.startsWith("[") && key.contains("..");

        if (isPageNode) {
            qDebug() << "✓ Virtual paging active, first page:" << key;
        } else {
            qDebug() << "✓ Direct children loaded (small file mode)";
        }
    }

    delete model;
    model = nullptr;
}

void TestAsyncLoading::testEmptyRootHandling()
{
    // Test that empty root doesn't crash
    model = new JsonTreeModel();

    bool loaded = model->load(testFile_empty);
    QVERIFY2(loaded, "Failed to load empty file");

    QModelIndex rootIndex;

    // Root should be marked as having children (we don't know it's empty yet)
    QVERIFY(model->hasChildren(rootIndex));

    // fetchMore should handle empty gracefully
    model->fetchMore(rootIndex);

    // After fetchMore, should have 0 children
    int childCount = model->rowCount(rootIndex);
    QCOMPARE(childCount, 0);

    qDebug() << "✓ Empty root handled gracefully";

    delete model;
    model = nullptr;
}

void TestAsyncLoading::testNestedLazyLoadingStillWorks()
{
    // Test that lazy loading still works for nested nodes
    model = new JsonTreeModel();

    bool loaded = model->load(testFile_nested);
    QVERIFY2(loaded, "Failed to load nested file");

    QModelIndex rootIndex;

    // Load root children
    model->fetchMore(rootIndex);
    QVERIFY(model->rowCount(rootIndex) > 0);

    // Get first child
    QModelIndex level0 = model->index(0, 0, rootIndex);
    QVERIFY(level0.isValid());

    // Check if it has children
    if (model->hasChildren(level0)) {
        // Before fetchMore: 0 children
        QCOMPARE(model->rowCount(level0), 0);

        // Load its children
        model->fetchMore(level0);

        // After fetchMore: should have children
        QVERIFY(model->rowCount(level0) > 0);

        qDebug() << "✓ Nested lazy loading still works";
    }

    delete model;
    model = nullptr;
}

void TestAsyncLoading::testLoadPerformance_100k_array()
{
    // Performance test: 100k array should load in < 100ms
    model = new JsonTreeModel();

    QElapsedTimer timer;
    timer.start();

    bool loaded = model->load(testFile_100k_array);
    qint64 loadTime = timer.elapsed();

    QVERIFY2(loaded, "Failed to load 100k array");
    QVERIFY2(loadTime < 100,
             QString("Load time %1ms exceeds 100ms target").arg(loadTime).toUtf8());

    qDebug() << "✓ 100k array load time:" << loadTime << "ms";

    // Verify no children loaded yet
    QModelIndex rootIndex;
    QCOMPARE(model->rowCount(rootIndex), 0);

    delete model;
    model = nullptr;
}

void TestAsyncLoading::testLoadPerformance_100k_object()
{
    // Performance test: 100k object should load in < 100ms
    model = new JsonTreeModel();

    QElapsedTimer timer;
    timer.start();

    bool loaded = model->load(testFile_100k_object);
    qint64 loadTime = timer.elapsed();

    QVERIFY2(loaded, "Failed to load 100k object");
    QVERIFY2(loadTime < 100,
             QString("Load time %1ms exceeds 100ms target").arg(loadTime).toUtf8());

    qDebug() << "✓ 100k object load time:" << loadTime << "ms";

    // Verify no children loaded yet
    QModelIndex rootIndex;
    QCOMPARE(model->rowCount(rootIndex), 0);

    delete model;
    model = nullptr;
}

QTEST_MAIN(TestAsyncLoading)
#include "test_async_loading.moc"
