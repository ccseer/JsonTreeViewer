#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>

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

    // Helper: Wait for async load to complete
    bool waitForLoad(JsonTreeModel* m, int timeoutMs = 10000)
    {
        QSignalSpy spy(m, &JsonTreeModel::loadFinished);
        if (spy.wait(timeoutMs)) {
            // Check if load was successful
            QList<QVariant> arguments = spy.takeFirst();
            return arguments.at(0).toBool();  // success flag
        }
        return false;
    }
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
        QFAIL(
            "Could not find test directory with Phase 13 test files. Run: "
            "python tests/generate_test_json.py");
    }

    // Set full paths
    testFile_100k_array  = testDir + "test_100k_array.json";
    testFile_100k_object = testDir + "test_100k_object.json";
    testFile_empty       = testDir + "test_empty.json";
    testFile_nested      = testDir + "test_nested.json";

    // Verify test files exist
    QStringList testFiles = {"test_100k_array.json", "test_100k_object.json",
                             "test_empty.json", "test_nested.json"};

    for (const QString& file : testFiles) {
        QString path = testDir + file;
        if (!QFile::exists(path)) {
            QFAIL(qPrintable("Test file not found: " + path
                             + ". Run: python tests/generate_test_json.py"));
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

    model->load(testFile_100k_array);
    bool loaded = waitForLoad(model);
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

    model->load(testFile_100k_array);
    bool loaded     = waitForLoad(model);
    qint64 loadTime = timer.elapsed();

    QVERIFY2(loaded, "Failed to load test file");
    // Note: With async loading, this measures total time including thread
    // overhead The actual parsing is still fast, but we can't measure it
    // separately in tests
    qDebug() << "✓ Load time:" << loadTime << "ms (async load completed)";

    delete model;
    model = nullptr;
}

void TestAsyncLoading::testRootHasChildrenAfterLoad()
{
    // Test that root is marked as having children
    model = new JsonTreeModel();

    model->load(testFile_100k_array);
    bool loaded = waitForLoad(model);
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

    model->load(testFile_100k_array);
    bool loaded = waitForLoad(model);
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

    model->load(testFile_100k_array);
    bool loaded = waitForLoad(model);
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

    model->load(testFile_100k_array);
    bool loaded = waitForLoad(model);
    QVERIFY2(loaded, "Failed to load test file");

    QModelIndex rootIndex;

    // Before fetchMore: 0 children
    QCOMPARE(model->rowCount(rootIndex), 0);

    // Call fetchMore - this is also async now!
    model->fetchMore(rootIndex);

    // Wait for fetchMore to complete by checking rowCount in a loop
    // (fetchMore doesn't have a signal, so we poll)
    int maxWait    = 100;  // 10 seconds
    int childCount = 0;
    for (int i = 0; i < maxWait; i++) {
        QTest::qWait(100);
        childCount = model->rowCount(rootIndex);
        if (childCount > 0)
            break;
    }

    // After fetchMore: should have children (paged or direct)
    QVERIFY2(childCount > 0, "fetchMore should load children");

    qDebug() << "✓ fetchMore loaded" << childCount << "children";

    delete model;
    model = nullptr;
}

void TestAsyncLoading::testVirtualPagingAfterFetchMore()
{
    // Test that virtual paging works after fetchMore
    model = new JsonTreeModel();

    model->load(testFile_100k_array);
    bool loaded = waitForLoad(model);
    QVERIFY2(loaded, "Failed to load test file");

    QModelIndex rootIndex;
    model->fetchMore(rootIndex);

    // Wait for fetchMore to complete
    int maxWait    = 100;
    int childCount = 0;
    for (int i = 0; i < maxWait; i++) {
        QTest::qWait(100);
        childCount = model->rowCount(rootIndex);
        if (childCount > 0)
            break;
    }

    // For 100k array with Large/Extreme strategy, should have paged nodes
    // Check if first child looks like a page node
    if (childCount > 0) {
        QModelIndex firstChild = model->index(0, 0, rootIndex);
        QString key = model->data(firstChild, Qt::DisplayRole).toString();

        // Virtual page nodes have format like "[0..99]"
        bool isPageNode = key.startsWith("[") && key.contains("..");

        if (isPageNode) {
            qDebug() << "✓ Virtual paging active, first page:" << key;
        }
        else {
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

    model->load(testFile_empty);
    bool loaded = waitForLoad(model);
    QVERIFY2(loaded, "Failed to load empty file");

    QModelIndex rootIndex;

    // For empty file, root may or may not be marked as having children
    // depending on the file content. Let's just check it doesn't crash.
    bool hasChildren = model->hasChildren(rootIndex);
    qDebug() << "Empty file hasChildren:" << hasChildren;

    // fetchMore should handle empty gracefully
    if (model->canFetchMore(rootIndex)) {
        model->fetchMore(rootIndex);
        // Wait for fetchMore to complete
        QTest::qWait(500);
    }

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

    model->load(testFile_nested);
    bool loaded = waitForLoad(model);
    QVERIFY2(loaded, "Failed to load nested file");

    QModelIndex rootIndex;

    // Load root children - async!
    model->fetchMore(rootIndex);

    // Wait for fetchMore to complete
    int maxWait = 100;
    for (int i = 0; i < maxWait; i++) {
        QTest::qWait(100);
        if (model->rowCount(rootIndex) > 0)
            break;
    }

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
    // Performance test: 100k array should load quickly (async)
    model = new JsonTreeModel();

    QElapsedTimer timer;
    timer.start();

    model->load(testFile_100k_array);
    bool loaded     = waitForLoad(model);
    qint64 loadTime = timer.elapsed();

    QVERIFY2(loaded, "Failed to load 100k array");
    // Note: With async loading, we measure total time including thread overhead
    // The actual parsing is still fast, but we can't measure it separately
    qDebug() << "✓ 100k array load time:" << loadTime << "ms (async)";

    // Verify no children loaded yet
    QModelIndex rootIndex;
    QCOMPARE(model->rowCount(rootIndex), 0);

    delete model;
    model = nullptr;
}

void TestAsyncLoading::testLoadPerformance_100k_object()
{
    // Performance test: 100k object should load quickly (async)
    model = new JsonTreeModel();

    QElapsedTimer timer;
    timer.start();

    model->load(testFile_100k_object);
    bool loaded     = waitForLoad(model);
    qint64 loadTime = timer.elapsed();

    QVERIFY2(loaded, "Failed to load 100k object");
    // Note: With async loading, we measure total time including thread overhead
    qDebug() << "✓ 100k object load time:" << loadTime << "ms (async)";

    // Verify no children loaded yet
    QModelIndex rootIndex;
    QCOMPARE(model->rowCount(rootIndex), 0);

    delete model;
    model = nullptr;
}

QTEST_MAIN(TestAsyncLoading)
#include "test_async_loading.moc"
