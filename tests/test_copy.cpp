#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

#include "../src/jsonnode.h"
#include "../src/jsontreemodel.h"
#include "../src/strategies/jsonstrategy.h"

class TestCopy : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Copy operations tests
    void testCopyKey();
    void testCopyValue();
    void testCopyPath();
    void testCopySubtreeScalar();
    void testCopySubtreeSmallObject();
    void testCopySubtreeSizeLimit();
    void testCopySubtreeInvalidIndex();
    void testCopyKeyValueScalar();
    void testCopyKeyValueObject();
    void testCopyKeyValueArrayFails();
    void testSupportedActionsSmall();

private:
    QString testDir;
    JsonTreeModel* model = nullptr;

    void createTestFile();

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
    void loadRootChildren(JsonTreeModel* m)
    {
        QModelIndex rootIndex;
        if (m->canFetchMore(rootIndex)) {
            m->fetchMore(rootIndex);
            // Wait for async fetchMore to complete
            // Poll until children are loaded (not just "Loading..."
            // placeholder)
            for (int i = 0; i < 100; ++i) {
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
};

void TestCopy::initTestCase()
{
    testDir = QDir::tempPath() + "/jsontreeviewer_copy_tests/";
    QDir dir;
    if (!dir.exists(testDir)) {
        dir.mkpath(testDir);
    }

    qDebug() << "Test directory:" << testDir;
    createTestFile();
}

void TestCopy::cleanupTestCase()
{
    if (model) {
        delete model;
        model = nullptr;
    }

    QDir dir(testDir);
    dir.removeRecursively();
}

void TestCopy::createTestFile()
{
    // Create a test JSON file
    QJsonObject nested;
    nested["inner_key"] = "inner_value";
    nested["number"]    = 42;

    QJsonObject root;
    root["name"]   = "test_item";
    root["value"]  = 123;
    root["nested"] = nested;
    root["flag"]   = true;

    QJsonDocument doc(root);
    QFile file(testDir + "test.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson(QJsonDocument::Compact));
        file.close();
    }
}

void TestCopy::testCopyKey()
{
    qDebug() << "\n=== Test: Copy Key ===";

    if (model)
        delete model;
    model = new JsonTreeModel(this);

    QString path = testDir + "test.json";
    model->load(path);
    QVERIFY(waitForLoad(model));

    // Load root children
    loadRootChildren(model);

    // Get first child (should be "name")
    QModelIndex firstChild = model->index(0, 0);
    QVERIFY(firstChild.isValid());

    QString key = model->getKey(firstChild);
    qDebug() << "Copied key:" << key;

    QVERIFY(!key.isEmpty());
    // The key should be one of the root object's keys
    QVERIFY(key == "name" || key == "value" || key == "nested"
            || key == "flag");
}

void TestCopy::testCopyValue()
{
    qDebug() << "\n=== Test: Copy Value ===";

    if (model)
        delete model;
    model = new JsonTreeModel(this);

    QString path = testDir + "test.json";
    model->load(path);
    QVERIFY(waitForLoad(model));

    // Load root children
    loadRootChildren(model);

    // Find "name" field
    QModelIndex nameIndex;
    for (int i = 0; i < model->rowCount(); ++i) {
        QModelIndex idx = model->index(i, 0);
        if (model->getKey(idx) == "name") {
            nameIndex = idx;
            break;
        }
    }

    QVERIFY(nameIndex.isValid());

    QString value = model->getValue(nameIndex);
    qDebug() << "Copied value:" << value;

    QCOMPARE(value, QString("test_item"));
}

void TestCopy::testCopyPath()
{
    qDebug() << "\n=== Test: Copy Path (JSON Pointer) ===";

    if (model)
        delete model;
    model = new JsonTreeModel(this);

    QString path = testDir + "test.json";
    model->load(path);
    QVERIFY(waitForLoad(model));

    // Load root children
    loadRootChildren(model);

    // Get first child
    QModelIndex firstChild = model->index(0, 0);
    QVERIFY(firstChild.isValid());

    QString jsonPath = model->getPath(firstChild);
    qDebug() << "Copied path:" << jsonPath;

    QVERIFY(!jsonPath.isEmpty());
    // Path should start with /
    QVERIFY(jsonPath.startsWith("/"));
}

void TestCopy::testCopySubtreeScalar()
{
    qDebug() << "\n=== Test: Copy Subtree (Scalar) ===";

    if (model)
        delete model;
    model = new JsonTreeModel(this);

    QString path = testDir + "test.json";
    model->load(path);
    QVERIFY(waitForLoad(model));

    // Load root children
    loadRootChildren(model);

    // Find "value" field (should be 123)
    QModelIndex valueIndex;
    for (int i = 0; i < model->rowCount(); ++i) {
        QModelIndex idx = model->index(i, 0);
        if (model->getKey(idx) == "value") {
            valueIndex = idx;
            break;
        }
    }

    QVERIFY(valueIndex.isValid());

    bool success = false;
    QString errorMsg;
    QString subtree = model->getSubtree(valueIndex, &success, &errorMsg);

    qDebug() << "Subtree:" << subtree;
    qDebug() << "Success:" << success;
    qDebug() << "Error:" << errorMsg;

    QVERIFY(success);
    QCOMPARE(subtree, QString("123"));
}

void TestCopy::testCopySubtreeSmallObject()
{
    qDebug() << "\n=== Test: Copy Subtree (Small Object) ===";

    if (model)
        delete model;
    model = new JsonTreeModel(this);

    QString path = testDir + "test.json";
    model->load(path);
    QVERIFY(waitForLoad(model));

    // Load root children
    loadRootChildren(model);

    // Find "nested" object
    QModelIndex nestedIndex;
    for (int i = 0; i < model->rowCount(); ++i) {
        QModelIndex idx = model->index(i, 0);
        if (model->getKey(idx) == "nested") {
            nestedIndex = idx;
            break;
        }
    }

    QVERIFY(nestedIndex.isValid());

    bool success = false;
    QString errorMsg;
    QString subtree = model->getSubtree(nestedIndex, &success, &errorMsg);

    qDebug() << "Subtree:" << subtree;
    qDebug() << "Success:" << success;

    QVERIFY(success);
    QVERIFY(!subtree.isEmpty());
    // Should contain the nested object's content
    QVERIFY(subtree.contains("inner_key") || subtree.contains("inner_value"));
}

void TestCopy::testCopySubtreeSizeLimit()
{
    qDebug() << "\n=== Test: Copy Subtree Size Limit ===";

    // Create a large file (> 10 MB)
    QString largePath = testDir + "large.json";
    QFile file(largePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write("{\"data\":\"");
        // Write 11 MB of data
        QByteArray padding(11 * 1024 * 1024, 'x');
        file.write(padding);
        file.write("\"}");
        file.close();
    }

    if (model)
        delete model;
    model = new JsonTreeModel(this);

    model->load(largePath);
    QVERIFY(waitForLoad(model));

    // Load root children
    loadRootChildren(model);

    // Try to copy the root (which is > 10 MB)
    QModelIndex rootChild = model->index(0, 0);
    QVERIFY(rootChild.isValid());

    bool success = false;
    QString errorMsg;
    QString subtree = model->getSubtree(rootChild, &success, &errorMsg);

    qDebug() << "Success:" << success;
    qDebug() << "Error:" << errorMsg;

    // Should fail due to size limit
    QVERIFY(!success);
    QVERIFY(!errorMsg.isEmpty());
    QVERIFY(errorMsg.contains("too large") || errorMsg.contains("Maximum"));
}

void TestCopy::testCopySubtreeInvalidIndex()
{
    qDebug() << "\n=== Test: Copy Subtree Invalid Index ===";

    if (model)
        delete model;
    model = new JsonTreeModel(this);

    QString path = testDir + "test.json";
    model->load(path);
    QVERIFY(waitForLoad(model));

    // Try to copy with invalid index
    QModelIndex invalidIndex;
    QVERIFY(!invalidIndex.isValid());

    bool success = false;
    QString errorMsg;
    QString subtree = model->getSubtree(invalidIndex, &success, &errorMsg);

    qDebug() << "Success:" << success;
    qDebug() << "Error:" << errorMsg;

    QVERIFY(!success);
    QVERIFY(!errorMsg.isEmpty());
}

void TestCopy::testCopyKeyValueScalar()
{
    qDebug() << "\n=== Test: Copy Key:Value (Scalar) ===";

    if (model)
        delete model;
    model = new JsonTreeModel(this);
    model->load(testDir + "test.json");
    QVERIFY(waitForLoad(model));

    // Load root children
    loadRootChildren(model);

    QModelIndex nameIndex;
    for (int i = 0; i < model->rowCount(); ++i) {
        QModelIndex idx = model->index(i, 0);
        if (model->getKey(idx) == "name") {
            nameIndex = idx;
            break;
        }
    }
    QVERIFY(nameIndex.isValid());

    bool success = false;
    QString errorMsg;
    QString kv = model->getKeyValue(nameIndex, &success, &errorMsg);
    qDebug() << "KeyValue:" << kv;

    QVERIFY(success);
    QCOMPARE(kv, QString("\"name\": \"test_item\""));
}

void TestCopy::testCopyKeyValueObject()
{
    qDebug() << "\n=== Test: Copy Key:Value (Object) ===";

    if (model)
        delete model;
    model = new JsonTreeModel(this);
    model->load(testDir + "test.json");
    QVERIFY(waitForLoad(model));

    // Load root children
    loadRootChildren(model);

    QModelIndex nestedIndex;
    for (int i = 0; i < model->rowCount(); ++i) {
        QModelIndex idx = model->index(i, 0);
        if (model->getKey(idx) == "nested") {
            nestedIndex = idx;
            break;
        }
    }
    QVERIFY(nestedIndex.isValid());

    bool success = false;
    QString errorMsg;
    QString kv = model->getKeyValue(nestedIndex, &success, &errorMsg);
    qDebug() << "KeyValue:" << kv;

    QVERIFY(success);
    QVERIFY(kv.startsWith("\"nested\": {"));
    QVERIFY(kv.contains("inner_key"));
}

void TestCopy::testCopyKeyValueArrayFails()
{
    qDebug() << "\n=== Test: Copy Key:Value (Array node - should fail) ===";

    // Build a JSON with a top-level array value
    QString arrayPath = testDir + "array.json";
    QFile f(arrayPath);
    if (f.open(QIODevice::WriteOnly)) {
        f.write("{\"items\":[1,2,3]}");
        f.close();
    }

    if (model)
        delete model;
    model = new JsonTreeModel(this);
    model->load(arrayPath);
    QVERIFY(waitForLoad(model));

    // Load root children
    loadRootChildren(model);

    QModelIndex arrIndex;
    for (int i = 0; i < model->rowCount(); ++i) {
        QModelIndex idx = model->index(i, 0);
        if (model->getKey(idx) == "items") {
            arrIndex = idx;
            break;
        }
    }
    QVERIFY(arrIndex.isValid());

    bool success = false;
    QString errorMsg;
    model->getKeyValue(arrIndex, &success, &errorMsg);
    qDebug() << "Success:" << success << "Error:" << errorMsg;

    QVERIFY(!success);
    QVERIFY(!errorMsg.isEmpty());
}

void TestCopy::testSupportedActionsSmall()
{
    qDebug() << "\n=== Test: Supported Actions (Small file) ===";

    if (model)
        delete model;
    model = new JsonTreeModel(this);
    model->load(testDir + "test.json");
    QVERIFY(waitForLoad(model));

    auto actions = model->supportedActions();
    using CA     = JsonViewerStrategy::CopyAction;
    QVERIFY(actions & CA::Key);
    QVERIFY(actions & CA::Value);
    QVERIFY(actions & CA::Path);
    QVERIFY(actions & CA::KeyValue);
    QVERIFY(actions & CA::Subtree);
}

QTEST_MAIN(TestCopy)
#include "test_copy.moc"
