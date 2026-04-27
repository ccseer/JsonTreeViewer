#include <QCoreApplication>
#include <QDebug>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QTest>
#include <memory>

#include "../src/navigation/searchworker.h"
#include "../src/strategies/jsonstrategy.h"

class TestSearch : public QObject {
    Q_OBJECT

private slots:
    void testBasicSearch();
    void testRegexSearch();
    void testDeepNesting();
    void testResultLimit();

private:
    std::shared_ptr<JsonViewerStrategy> createStrategy(const char* json)
    {
        QTemporaryFile tempFile;
        if (!tempFile.open())
            return nullptr;
        tempFile.write(json);
        tempFile.close();

        auto strategy = JsonViewerStrategy::createStrategy(tempFile.size());
        if (strategy && strategy->initialize(tempFile.fileName())) {
            return strategy;
        }
        return nullptr;
    }
};

void TestSearch::testBasicSearch()
{
    const char* json = R"({"a": 1, "b": "hello", "c": {"d": "world"}})";
    auto strategy    = createStrategy(json);
    QVERIFY(strategy != nullptr);

    SearchQuery query;
    query.text          = "world";
    query.type          = SearchType::All;
    query.caseSensitive = false;
    query.useRegex      = false;

    SearchWorker* worker = new SearchWorker(strategy, query);
    QSignalSpy spy(worker, &SearchWorker::resultsFound);

    worker->process();

    QCOMPARE(spy.count(), 1);
    auto results = spy.at(0).at(0).value<QVector<SearchResult>>();
    QCOMPARE(results.size(), 1);
    QCOMPARE(results[0].path, QString("/c/d"));
    QCOMPARE(results[0].value, QString("world"));
}

void TestSearch::testRegexSearch()
{
    const char* json = R"({"apple": 1, "apply": 2, "orange": 3})";
    auto strategy    = createStrategy(json);
    QVERIFY(strategy != nullptr);

    SearchQuery query;
    query.text     = "app.*";
    query.type     = SearchType::Key;
    query.useRegex = true;

    SearchWorker* worker = new SearchWorker(strategy, query);
    QSignalSpy spy(worker, &SearchWorker::resultsFound);

    worker->process();

    QCOMPARE(spy.count(), 1);
    auto results = spy.at(0).at(0).value<QVector<SearchResult>>();
    QCOMPARE(results.size(), 2);  // apple and apply
}

void TestSearch::testDeepNesting()
{
    const char* json = R"({"level1": {"level2": {"level3": "found_me"}}})";
    auto strategy    = createStrategy(json);
    QVERIFY(strategy != nullptr);

    SearchQuery query;
    query.text = "found_me";
    query.type = SearchType::Value;

    SearchWorker* worker = new SearchWorker(strategy, query);
    QSignalSpy spy(worker, &SearchWorker::resultsFound);

    worker->process();

    QVERIFY(spy.count() > 0);
    auto results = spy.at(0).at(0).value<QVector<SearchResult>>();
    QCOMPARE(results.size(), 1);
    QCOMPARE(results[0].path, QString("/level1/level2/level3"));
}

void TestSearch::testResultLimit()
{
    QByteArray json = "[";
    for (int i = 0; i < 15000; ++i) {
        json += "1";
        if (i < 14999)
            json += ",";
    }
    json += "]";

    auto strategy = createStrategy(json.constData());
    QVERIFY(strategy != nullptr);

    SearchQuery query;
    query.text = "1";
    query.type = SearchType::Value;

    SearchWorker* worker = new SearchWorker(strategy, query);
    QSignalSpy spy(worker, &SearchWorker::resultsFound);

    worker->process();

    int totalFound = 0;
    for (int i = 0; i < spy.count(); ++i) {
        totalFound += spy.at(i).at(0).value<QVector<SearchResult>>().size();
    }

    QCOMPARE(totalFound, 10000);
}

QTEST_MAIN(TestSearch)
#include "test_search.moc"
