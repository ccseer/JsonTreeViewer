#pragma once

#include <simdjson.h>

#include <QElapsedTimer>
#include <QObject>
#include <QRegularExpression>
#include <QString>
#include <QVector>
#include <memory>

#include "../strategies/jsonstrategy.h"

enum class SearchType { Key, Value, All, Path };

struct SearchQuery {
    QString text;
    SearchType type    = SearchType::All;
    bool caseSensitive = false;
    bool useRegex      = false;
};

struct SearchResult {
    QString key;
    QString value;
    QString path;
    char type = '?';  // 'o', 'a', 's', 'n', 'b', 'u' (null)
};

Q_DECLARE_METATYPE(SearchResult)
Q_DECLARE_METATYPE(QVector<SearchResult>)

class SearchWorker : public QObject {
    Q_OBJECT
public:
    explicit SearchWorker(std::shared_ptr<JsonViewerStrategy> strategy,
                          const SearchQuery& query,
                          QObject* parent = nullptr);

public slots:
    void process();

signals:
    void resultsFound(const QVector<SearchResult>& results);
    void progressUpdated(int percentage);
    void finished(bool success);

private:
    void searchRecursive(simdjson::ondemand::value val,
                         QString currentPath,
                         const QString& currentKey,
                         const char* basePtr);
    bool matches(const QString& text);
    void emitBatch(bool force = false);

    std::shared_ptr<JsonViewerStrategy> m_strategy;
    const char* m_data;
    size_t m_size;
    SearchQuery m_query;
    QRegularExpression m_re;

    QVector<SearchResult> m_batch;
    QElapsedTimer m_batchTimer;
    int m_lastProgress               = -1;
    int m_totalResults               = 0;
    static constexpr int MAX_RESULTS = 10000;
};
