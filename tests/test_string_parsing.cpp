#include <QString>
#include <QTemporaryFile>
#include <QTest>

#include "../src/jsonnode.h"
#include "../src/strategies/smallfilestrategy.h"

class TestStringParsing : public QObject {
    Q_OBJECT

private slots:
    void testSimpleString();
    void testStringWithEscapedQuote();
    void testStringWithBackslash();
    void testStringWithDoubleBackslash();
    void testEmptyString();
    void testLongString();
    void testStringWithNewlines();
    void testStringWithUnicode();
};

void TestStringParsing::testSimpleString()
{
    const char* json = R"({"name": "John"})";
    SmallFileStrategy strategy;

    // Create a temporary file
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    tempFile.write(json);
    tempFile.close();

    QVERIFY(strategy.initialize(tempFile.fileName()));

    QString pointer;
    quint64 offset, length;
    quint32 child_count;
    strategy.getRootMetadata(pointer, offset, length, child_count);

    auto children = strategy.extractChildren(pointer, offset, length);
    QCOMPARE(children.size(), 1);
    QCOMPARE(children[0]->key, QString("name"));
    QCOMPARE(children[0]->value, QString("John"));

    qDeleteAll(children);
}

void TestStringParsing::testStringWithEscapedQuote()
{
    const char* json = R"({"text": "He said \"Hello\""})";
    SmallFileStrategy strategy;

    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    tempFile.write(json);
    tempFile.close();

    QVERIFY(strategy.initialize(tempFile.fileName()));

    QString pointer;
    quint64 offset, length;
    quint32 child_count;
    strategy.getRootMetadata(pointer, offset, length, child_count);

    auto children = strategy.extractChildren(pointer, offset, length);
    QCOMPARE(children.size(), 1);
    QCOMPARE(children[0]->key, QString("text"));
    QCOMPARE(children[0]->value, QString("He said \\\"Hello\\\""));

    qDeleteAll(children);
}

void TestStringParsing::testStringWithBackslash()
{
    const char* json = R"({"path": "C:\\Users\\test"})";
    SmallFileStrategy strategy;

    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    tempFile.write(json);
    tempFile.close();

    QVERIFY(strategy.initialize(tempFile.fileName()));

    QString pointer;
    quint64 offset, length;
    quint32 child_count;
    strategy.getRootMetadata(pointer, offset, length, child_count);

    auto children = strategy.extractChildren(pointer, offset, length);
    QCOMPARE(children.size(), 1);
    QCOMPARE(children[0]->key, QString("path"));
    QCOMPARE(children[0]->value, QString("C:\\\\Users\\\\test"));

    qDeleteAll(children);
}

void TestStringParsing::testStringWithDoubleBackslash()
{
    const char* json = R"({"end": "test\\"})";
    SmallFileStrategy strategy;

    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    tempFile.write(json);
    tempFile.close();

    QVERIFY(strategy.initialize(tempFile.fileName()));

    QString pointer;
    quint64 offset, length;
    quint32 child_count;
    strategy.getRootMetadata(pointer, offset, length, child_count);

    auto children = strategy.extractChildren(pointer, offset, length);
    QCOMPARE(children.size(), 1);
    QCOMPARE(children[0]->key, QString("end"));
    QCOMPARE(children[0]->value, QString("test\\\\"));

    qDeleteAll(children);
}

void TestStringParsing::testEmptyString()
{
    const char* json = R"({"empty": ""})";
    SmallFileStrategy strategy;

    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    tempFile.write(json);
    tempFile.close();

    QVERIFY(strategy.initialize(tempFile.fileName()));

    QString pointer;
    quint64 offset, length;
    quint32 child_count;
    strategy.getRootMetadata(pointer, offset, length, child_count);

    auto children = strategy.extractChildren(pointer, offset, length);
    QCOMPARE(children.size(), 1);
    QCOMPARE(children[0]->key, QString("empty"));
    QCOMPARE(children[0]->value, QString(""));

    qDeleteAll(children);
}

void TestStringParsing::testLongString()
{
    // Create a string longer than 100 characters
    QString longText(150, 'x');
    QString json = QString(R"({"long": "%1"})").arg(longText);

    SmallFileStrategy strategy;

    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    tempFile.write(json.toUtf8());
    tempFile.close();

    QVERIFY(strategy.initialize(tempFile.fileName()));

    QString pointer;
    quint64 offset, length;
    quint32 child_count;
    strategy.getRootMetadata(pointer, offset, length, child_count);

    auto children = strategy.extractChildren(pointer, offset, length);
    QCOMPARE(children.size(), 1);
    QCOMPARE(children[0]->key, QString("long"));
    // Should be truncated to 100 chars + "..."
    QVERIFY(children[0]->value.endsWith("..."));
    QCOMPARE(children[0]->value.length(), 103);

    qDeleteAll(children);
}

void TestStringParsing::testStringWithNewlines()
{
    const char* json = R"({"text": "line1\nline2"})";
    SmallFileStrategy strategy;

    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    tempFile.write(json);
    tempFile.close();

    QVERIFY(strategy.initialize(tempFile.fileName()));

    QString pointer;
    quint64 offset, length;
    quint32 child_count;
    strategy.getRootMetadata(pointer, offset, length, child_count);

    auto children = strategy.extractChildren(pointer, offset, length);
    QCOMPARE(children.size(), 1);
    QCOMPARE(children[0]->key, QString("text"));
    QCOMPARE(children[0]->value, QString("line1\\nline2"));

    qDeleteAll(children);
}

void TestStringParsing::testStringWithUnicode()
{
    const char* json = R"({"emoji": "Hello 😀 World"})";
    SmallFileStrategy strategy;

    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    tempFile.write(json);
    tempFile.close();

    QVERIFY(strategy.initialize(tempFile.fileName()));

    QString pointer;
    quint64 offset, length;
    quint32 child_count;
    strategy.getRootMetadata(pointer, offset, length, child_count);

    auto children = strategy.extractChildren(pointer, offset, length);
    QCOMPARE(children.size(), 1);
    QCOMPARE(children[0]->key, QString("emoji"));
    QVERIFY(children[0]->value.contains("😀"));

    qDeleteAll(children);
}

QTEST_MAIN(TestStringParsing)
#include "test_string_parsing.moc"
