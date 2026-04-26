#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QTest>

#include "../src/jsontreemodel.h"
#include "../src/strategies/jsonstrategy.h"

class TestErrors : public QObject {
    Q_OBJECT

private slots:
    void testMissingQuote();
    void testTrailingComma();
    void testInvalidValue();
    void testTruncatedJson();

private:
    bool waitForLoad(JsonTreeModel* m, int timeoutMs = 5000)
    {
        QSignalSpy spy(m, &JsonTreeModel::loadFinished);
        if (spy.wait(timeoutMs)) {
            QList<QVariant> arguments = spy.takeFirst();
            return arguments.at(0).toBool();
        }
        return false;
    }
};

void TestErrors::testMissingQuote()
{
    QTemporaryFile file;
    if (file.open()) {
        file.write("{\"key\": \"value}");  // Missing closing quote
        file.close();

        JsonTreeModel model;
        model.load(file.fileName());
        bool success = waitForLoad(&model);

        QVERIFY(!success);
        const auto* metrics = model.metrics();
        QVERIFY(metrics != nullptr);
        QVERIFY(!metrics->parseError.isEmpty());
        // Exact error message depends on simdjson version/platform,
        // but it should contain "quote" or "string" or generic error
        qDebug() << "Error message:" << metrics->parseError;
        qDebug() << "Error context:" << metrics->errorContext;
        QVERIFY(!metrics->errorContext.isEmpty());
    }
}

void TestErrors::testTrailingComma()
{
    QTemporaryFile file;
    if (file.open()) {
        // More obviously broken trailing comma (missing value after it)
        file.write("{\"a\": 1, \"b\": }");
        file.close();

        JsonTreeModel model;
        model.load(file.fileName());
        bool success = waitForLoad(&model);

        QVERIFY(!success);
        const auto* metrics = model.metrics();
        QVERIFY(metrics != nullptr);
        QVERIFY(!metrics->parseError.isEmpty());
        qDebug() << "Error message:" << metrics->parseError;
        qDebug() << "Error context:" << metrics->errorContext;
    }
}

void TestErrors::testInvalidValue()
{
    QTemporaryFile file;
    if (file.open()) {
        // Invalid tokens that look like nothing in JSON
        file.write("{\"a\": 1, \"b\": #@!}");
        file.close();

        JsonTreeModel model;
        model.load(file.fileName());
        bool success = waitForLoad(&model);

        QVERIFY(!success);
        const auto* metrics = model.metrics();
        QVERIFY(metrics != nullptr);
        QVERIFY(!metrics->parseError.isEmpty());
        qDebug() << "Error message:" << metrics->parseError;
        qDebug() << "Error context:" << metrics->errorContext;
    }
}

void TestErrors::testTruncatedJson()
{
    QTemporaryFile file;
    if (file.open()) {
        file.write("{\"a\": 1, \"b\": ");  // Truncated
        file.close();

        JsonTreeModel model;
        model.load(file.fileName());
        bool success = waitForLoad(&model);

        QVERIFY(!success);
        const auto* metrics = model.metrics();
        QVERIFY(metrics != nullptr);
        QVERIFY(!metrics->parseError.isEmpty());
        qDebug() << "Error message:" << metrics->parseError;
        qDebug() << "Error context:" << metrics->errorContext;
    }
}

QTEST_MAIN(TestErrors)
#include "test_errors.moc"
