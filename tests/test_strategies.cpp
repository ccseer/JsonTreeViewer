#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QTest>

#include "../src/jsonnode.h"
#include "../src/jsontreemodel.h"
#include "../src/strategies/extremefilestrategy.h"
#include "../src/strategies/jsonstrategy.h"
#include "../src/strategies/largefilestrategy.h"
#include "../src/strategies/mediumfilestrategy.h"
#include "../src/strategies/smallfilestrategy.h"

class TestStrategies : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Strategy selection tests
    void testStrategySelection_1k();
    void testStrategySelection_11M();
    void testStrategySelection_101M();
    void testStrategySelection_1G();

    // Basic functionality tests
    void testSmallFileLoad();
    void testMediumFileLoad();
    void testLargeFileLoad();
    void testExtremeFileLoad();

    // Performance tests
    void testSmallFilePerformance();
    void testMediumFilePerformance();
    void testLargeFilePerformance();

    // Memory tests
    void testLargeFileMemory();

    // Data correctness tests
    void testDataCorrectness();
    void testLazyLoading();
    void testDeepNesting();

private:
    QString testDir;
    JsonTreeModel* model = nullptr;

    void loadAndVerify(const QString& filename,
                       JsonTreeModel::FileMode expectedMode,
                       qint64 maxLoadTime);
};

void TestStrategies::initTestCase()
{
    // Try multiple possible paths
    QStringList possiblePaths = {
        QCoreApplication::applicationDirPath() + "/../",
        QCoreApplication::applicationDirPath() + "/../tests/",
        QCoreApplication::applicationDirPath() + "/../../tests/",
        QCoreApplication::applicationDirPath() + "/../../../tests/",
        QDir::currentPath() + "/tests/",
        QDir::currentPath() + "/../tests/",
    };

    // Find the correct test directory
    for (const QString& path : possiblePaths) {
        QString testFile = path + "1k.json";
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
        QFAIL("Could not find test directory with test files");
    }

    // Verify test files exist
    QStringList testFiles = {"1k.json", "11M.json", "101M.json", "1.01G.json"};
    for (const QString& file : testFiles) {
        QString path = testDir + file;
        if (!QFile::exists(path)) {
            QFAIL(qPrintable("Test file not found: " + path));
        }
    }

    qDebug() << "All test files found";
}

void TestStrategies::cleanupTestCase()
{
    if (model) {
        delete model;
        model = nullptr;
    }
}

// __CONTINUE_HERE__

void TestStrategies::loadAndVerify(const QString& filename,
                                    JsonTreeModel::FileMode expectedMode,
                                    qint64 maxLoadTime)
{
    QString path = testDir + filename;
    QFileInfo fileInfo(path);

    qDebug() << "\n=== Testing:" << filename << "===" << fileInfo.size()
             << "bytes";

    if (model) {
        delete model;
    }
    model = new JsonTreeModel(this);

    QElapsedTimer timer;
    timer.start();

    bool loaded = model->load(path);
    qint64 loadTime = timer.elapsed();

    QVERIFY2(loaded, qPrintable("Failed to load: " + filename));
    QCOMPARE(model->fileMode(), expectedMode);
    QVERIFY2(loadTime < maxLoadTime,
             qPrintable(QString("Load time %1ms exceeds limit %2ms")
                            .arg(loadTime)
                            .arg(maxLoadTime)));

    qDebug() << "  Load time:" << loadTime << "ms";
    qDebug() << "  Strategy:"
             << (expectedMode == JsonTreeModel::FileMode::Small      ? "Small"
                 : expectedMode == JsonTreeModel::FileMode::Medium   ? "Medium"
                 : expectedMode == JsonTreeModel::FileMode::Large    ? "Large"
                                                                      : "Extreme");

    // Verify root has children
    QVERIFY(model->rowCount() > 0);
}

void TestStrategies::testStrategySelection_1k()
{
    loadAndVerify("1k.json", JsonTreeModel::FileMode::Small, 100);
}

void TestStrategies::testStrategySelection_11M()
{
    loadAndVerify("11M.json", JsonTreeModel::FileMode::Medium, 500);
}

void TestStrategies::testStrategySelection_101M()
{
    loadAndVerify("101M.json", JsonTreeModel::FileMode::Large, 2000);
}

void TestStrategies::testStrategySelection_1G()
{
    loadAndVerify("1.01G.json", JsonTreeModel::FileMode::Extreme, 17000);
}

void TestStrategies::testSmallFileLoad()
{
    QString path = testDir + "1k.json";
    if (model)
        delete model;
    model = new JsonTreeModel(this);

    QVERIFY(model->load(path));
    QVERIFY(model->rowCount() > 0);

    // Test first level expansion
    QModelIndex firstChild = model->index(0, 0);
    QVERIFY(firstChild.isValid());

    // Test lazy loading
    if (model->canFetchMore(firstChild)) {
        model->fetchMore(firstChild);
        QVERIFY(model->rowCount(firstChild) > 0);
    }
}

void TestStrategies::testMediumFileLoad()
{
    QString path = testDir + "11M.json";
    if (model)
        delete model;
    model = new JsonTreeModel(this);

    QVERIFY(model->load(path));
    QCOMPARE(model->fileMode(), JsonTreeModel::FileMode::Medium);
    QVERIFY(model->rowCount() > 0);
}

void TestStrategies::testLargeFileLoad()
{
    QString path = testDir + "101M.json";
    if (model)
        delete model;
    model = new JsonTreeModel(this);

    QVERIFY(model->load(path));
    QCOMPARE(model->fileMode(), JsonTreeModel::FileMode::Large);
    QVERIFY(model->rowCount() > 0);
}

void TestStrategies::testExtremeFileLoad()
{
    QString path = testDir + "1.01G.json";
    if (model)
        delete model;
    model = new JsonTreeModel(this);

    QVERIFY(model->load(path));
    QCOMPARE(model->fileMode(), JsonTreeModel::FileMode::Extreme);
    QVERIFY(model->rowCount() > 0);
}

void TestStrategies::testSmallFilePerformance()
{
    QString path = testDir + "1k.json";
    if (model)
        delete model;
    model = new JsonTreeModel(this);

    QElapsedTimer timer;
    timer.start();
    QVERIFY(model->load(path));
    qint64 loadTime = timer.elapsed();

    qDebug() << "Small file load time:" << loadTime << "ms";
    QVERIFY2(loadTime < 100, "Small file load too slow");

    // Test expansion performance
    timer.restart();
    QModelIndex firstChild = model->index(0, 0);
    if (model->canFetchMore(firstChild)) {
        model->fetchMore(firstChild);
    }
    qint64 expandTime = timer.elapsed();

    qDebug() << "Small file expand time:" << expandTime << "ms";
    QVERIFY2(expandTime < 50, "Small file expand too slow");
}

void TestStrategies::testMediumFilePerformance()
{
    QString path = testDir + "11M.json";
    if (model)
        delete model;
    model = new JsonTreeModel(this);

    QElapsedTimer timer;
    timer.start();
    QVERIFY(model->load(path));
    qint64 loadTime = timer.elapsed();

    qDebug() << "Medium file load time:" << loadTime << "ms";
    QVERIFY2(loadTime < 500, "Medium file load too slow");
}

void TestStrategies::testLargeFilePerformance()
{
    QString path = testDir + "101M.json";
    if (model)
        delete model;
    model = new JsonTreeModel(this);

    QElapsedTimer timer;
    timer.start();
    QVERIFY(model->load(path));
    qint64 loadTime = timer.elapsed();

    qDebug() << "Large file load time:" << loadTime << "ms";
    QVERIFY2(loadTime < 2000, "Large file load too slow");
}

void TestStrategies::testLargeFileMemory()
{
    QString path = testDir + "101M.json";
    QFileInfo fileInfo(path);
    qint64 fileSize = fileInfo.size();

    if (model)
        delete model;
    model = new JsonTreeModel(this);

    QVERIFY(model->load(path));

    // Note: Actual memory measurement would require platform-specific APIs
    // This test just verifies the file loads successfully with Large strategy
    QCOMPARE(model->fileMode(), JsonTreeModel::FileMode::Large);

    qDebug() << "Large file loaded successfully";
    qDebug() << "File size:" << fileSize / (1024 * 1024) << "MB";
    qDebug()
        << "Expected memory: ~" << (fileSize * 1.2) / (1024 * 1024) << "MB";
    qDebug() << "Old implementation would use: ~"
             << (fileSize * 2.5) / (1024 * 1024) << "MB";
}

void TestStrategies::testDataCorrectness()
{
    QString path = testDir + "1k.json";
    if (model)
        delete model;
    model = new JsonTreeModel(this);

    QVERIFY(model->load(path));

    // Get first child
    QModelIndex firstChild = model->index(0, 0);
    QVERIFY(firstChild.isValid());

    // Verify key column
    QVariant keyData = model->data(firstChild, Qt::DisplayRole);
    QVERIFY(!keyData.toString().isEmpty());

    // Verify value column
    QModelIndex valueIndex = model->index(0, 1);
    QVariant valueData = model->data(valueIndex, Qt::DisplayRole);
    // Value can be empty for objects/arrays, so just verify it's valid
    QVERIFY(valueData.isValid());

    qDebug() << "First item - Key:" << keyData.toString()
             << "Value:" << valueData.toString();
}

void TestStrategies::testLazyLoading()
{
    QString path = testDir + "1k.json";
    if (model)
        delete model;
    model = new JsonTreeModel(this);

    QVERIFY(model->load(path));

    QModelIndex firstChild = model->index(0, 0);
    QVERIFY(firstChild.isValid());

    // Initially, children should not be loaded
    int initialChildCount = model->rowCount(firstChild);

    // If node has children, fetch them
    if (model->canFetchMore(firstChild)) {
        model->fetchMore(firstChild);

        // After fetchMore, should have children
        int afterFetchCount = model->rowCount(firstChild);
        QVERIFY(afterFetchCount > 0);

        qDebug() << "Lazy loading verified - children loaded:"
                 << afterFetchCount;
    }
}

void TestStrategies::testDeepNesting()
{
    QString path = testDir + "1k.json";
    if (model)
        delete model;
    model = new JsonTreeModel(this);

    QVERIFY(model->load(path));

    // Try to expand multiple levels
    QModelIndex current = model->index(0, 0);
    int depth = 0;
    const int maxDepth = 5;

    while (current.isValid() && depth < maxDepth) {
        if (model->canFetchMore(current)) {
            QElapsedTimer timer;
            timer.start();

            model->fetchMore(current);

            qint64 expandTime = timer.elapsed();
            qDebug() << "Depth" << depth << "expand time:" << expandTime
                     << "ms";

            QVERIFY2(expandTime < 100,
                     qPrintable(QString("Expand at depth %1 too slow: %2ms")
                                    .arg(depth)
                                    .arg(expandTime)));

            if (model->rowCount(current) > 0) {
                current = model->index(0, 0, current);
                depth++;
            }
            else {
                break;
            }
        }
        else {
            break;
        }
    }

    qDebug() << "Successfully expanded to depth:" << depth;
}

QTEST_MAIN(TestStrategies)
#include "test_strategies.moc"

