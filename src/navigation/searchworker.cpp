#include "searchworker.h"

#include <QScopeGuard>

#define qprintt qprint << "[SearchWorker]"

SearchWorker::SearchWorker(std::shared_ptr<JsonViewerStrategy> strategy,
                           const SearchQuery& query,
                           QObject* parent)
    : QObject(parent), m_strategy(strategy), m_query(query)
{
    qprintt << this;
    m_data = strategy->dataPtr();
    m_size = strategy->dataSize();

    // Pre-compile Regex if needed
    if (m_query.useRegex && !m_query.text.isEmpty()) {
        QRegularExpression::PatternOptions options
            = QRegularExpression::NoPatternOption;
        if (!m_query.caseSensitive)
            options |= QRegularExpression::CaseInsensitiveOption;
        m_re = QRegularExpression(m_query.text, options);

        if (!m_re.isValid()) {
            qprintt << "Invalid regex:" << m_re.errorString();
        }
    }
}

SearchWorker::~SearchWorker()
{
    qprintt << "~" << this;
}

void SearchWorker::process()
{
    bool success = false;
    // Chained cleanup trigger
    QScopeGuard _cleanup([this, &success]() {
        qprintt << "Cleaning up, calling deleteLater" << success;
        emit finished(success);
        this->deleteLater();
    });

    qprintt << "SearchWorker::process enter for:" << m_query.text
            << QThread::currentThread();

    if (!m_data || m_size == 0) {
        qprintt << "No data to search";
        return;
    }
    if (m_query.useRegex && !m_re.isValid()) {
        qprintt << "Cannot perform search due to invalid regex";
        return;
    }

    if (QThread::currentThread()->isInterruptionRequested()) {
        qprintt << "Search interrupted before start";
        return;
    }

    try {
        simdjson::ondemand::parser parser;
        // NO COPY: Use m_data directly as the strategy guarantees it's padded.
        simdjson::ondemand::document doc;
        if (auto e
            = parser
                  .iterate(m_data, m_size, m_size + simdjson::SIMDJSON_PADDING)
                  .get(doc)) {
            qprintt << "SearchWorker: Simdjson error:"
                    << simdjson::error_message(e);
            return;
        }

        if (QThread::currentThread()->isInterruptionRequested()) {
            qprintt << "Search interrupted after parsing";
            return;
        }

        m_batchTimer.start();
        m_searchTimer.start();
        m_totalResults = 0;
        m_lastProgress = -1;

        simdjson::ondemand::value val;
        if (doc.get_value().get(val) == simdjson::SUCCESS) {
            searchRecursive(val, "", "", m_data);
        }

        if (QThread::currentThread()->isInterruptionRequested()) {
            qprintt << "Search interrupted after searchRecursive";
            return;
        }

        emitBatch(true);
        success = true;
    }
    catch (const simdjson::simdjson_error& e) {
        qprintt << "simdjson error:" << e.what();
    }
    catch (const std::exception& e) {
        qprintt << "Exception:" << e.what();
    }
    catch (...) {
        qprintt << "Unknown exception";
    }
}

void SearchWorker::searchRecursive(simdjson::ondemand::value val,
                                   QString currentPath,
                                   const QString& currentKey,
                                   const char* basePtr)
{
    if (QThread::currentThread()->isInterruptionRequested())
        return;
    if (m_totalResults >= MAX_RESULTS) {
        emit limitReached(MAX_RESULTS);
        return;
    }
    if (m_searchTimer.elapsed() > SEARCH_TIMEOUT_MS) {
        qprintt << "Search timeout after" << SEARCH_TIMEOUT_MS << "ms";
        return;
    }

    simdjson::ondemand::json_type type;
    if (val.type().get(type) != simdjson::SUCCESS)
        return;

    std::string_view tok = val.raw_json_token();
    if (!tok.empty()) {
        const char* currentPtr = tok.data();
        if (currentPtr >= basePtr && currentPtr < basePtr + m_size) {
            int progress = 0;
            if (m_size > 0) {
                // Use qint64 to prevent overflow during multiplication
                progress = static_cast<int>(
                    (static_cast<qint64>(currentPtr - basePtr) * 100) / m_size);
            }
            if (progress > m_lastProgress) {
                m_lastProgress = progress;
                emit progressUpdated(progress);
            }
        }
    }

    char typeChar = '?';
    QString valueSnippet;
    bool isContainer = false;

    switch (type) {
    case simdjson::ondemand::json_type::object:
        typeChar     = 'o';
        isContainer  = true;
        valueSnippet = "{...}";
        break;
    case simdjson::ondemand::json_type::array:
        typeChar     = 'a';
        isContainer  = true;
        valueSnippet = "[...]";
        break;
    case simdjson::ondemand::json_type::string: {
        typeChar = 's';
        std::string_view s;
        if (val.get_string().get(s) == simdjson::SUCCESS)
            valueSnippet = QString::fromUtf8(s.data(), s.size());
        break;
    }
    case simdjson::ondemand::json_type::number: {
        typeChar = 'n';
        std::string_view raw_num;
        if (val.raw_json().get(raw_num) == simdjson::SUCCESS)
            valueSnippet = QString::fromUtf8(raw_num.data(), raw_num.size());
        break;
    }
    case simdjson::ondemand::json_type::boolean: {
        typeChar = 'b';
        bool b;
        if (val.get_bool().get(b) == simdjson::SUCCESS)
            valueSnippet = b ? "true" : "false";
        break;
    }
    case simdjson::ondemand::json_type::null:
        typeChar     = 'u';
        valueSnippet = "null";
        break;
    }

    if (!currentKey.isEmpty()
        && (m_query.type == SearchType::Key
            || m_query.type == SearchType::All)) {
        if (matches(currentKey)) {
            m_batch.append({currentKey, valueSnippet, currentPath, typeChar});
            m_totalResults++;
            if (m_totalResults >= MAX_RESULTS) {
                emitBatch(true);
                emit limitReached(MAX_RESULTS);
                return;
            }
            emitBatch();
        }
    }

    if (!isContainer
        && (m_query.type == SearchType::Value
            || m_query.type == SearchType::All)) {
        if (matches(valueSnippet)) {
            m_batch.append({currentKey, valueSnippet, currentPath, typeChar});
            m_totalResults++;
            if (m_totalResults >= MAX_RESULTS) {
                emitBatch(true);
                emit limitReached(MAX_RESULTS);
                return;
            }
            emitBatch();
        }
    }

    if (m_query.type == SearchType::Path && matches(currentPath)) {
        m_batch.append({currentKey, valueSnippet, currentPath, typeChar});
        m_totalResults++;
        if (m_totalResults >= MAX_RESULTS) {
            emitBatch(true);
            emit limitReached(MAX_RESULTS);
            return;
        }
        emitBatch();
    }

    if (m_totalResults >= MAX_RESULTS)
        return;

    if (type == simdjson::ondemand::json_type::object) {
        simdjson::ondemand::object obj;
        if (val.get_object().get(obj) == simdjson::SUCCESS) {
            for (auto field : obj) {
                if (QThread::currentThread()->isInterruptionRequested()) {
                    qprintt << "Search interrupted during object iteration";
                    return;
                }
                std::string_view key_view;
                if (field.unescaped_key().get(key_view) == simdjson::SUCCESS) {
                    QString key
                        = QString::fromUtf8(key_view.data(), key_view.size());
                    QString escapedKey = JsonPointer::escape(key);
                    QString nextPath   = currentPath + "/" + escapedKey;
                    simdjson::ondemand::value nextVal;
                    if (field.value().get(nextVal) == simdjson::SUCCESS)
                        searchRecursive(nextVal, nextPath, key, basePtr);
                }
                if (m_totalResults >= MAX_RESULTS)
                    return;
            }
        }
    }
    else if (type == simdjson::ondemand::json_type::array) {
        simdjson::ondemand::array arr;
        if (val.get_array().get(arr) == simdjson::SUCCESS) {
            int index = 0;
            for (auto element : arr) {
                if (QThread::currentThread()->isInterruptionRequested()) {
                    qprintt << "Search interrupted during array iteration";
                    return;
                }
                QString nextPath = currentPath + "/" + QString::number(index);
                simdjson::ondemand::value nextVal;
                if (element.get(nextVal) == simdjson::SUCCESS)
                    searchRecursive(nextVal, nextPath, QString::number(index),
                                    basePtr);
                index++;
                if (m_totalResults >= MAX_RESULTS)
                    return;
            }
        }
    }
}

bool SearchWorker::matches(const QString& text)
{
    if (m_query.text.isEmpty())
        return false;
    if (m_query.useRegex)
        return m_re.isValid() && m_re.match(text).hasMatch();
    auto cs = m_query.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    return text.contains(m_query.text, cs);
}

void SearchWorker::emitBatch(bool force)
{
    if (m_batch.isEmpty())
        return;
    if (force || m_batch.size() >= BATCH_SIZE
        || m_batchTimer.elapsed() >= BATCH_INTERVAL_MS) {
        emit resultsFound(m_batch);
        m_batch.clear();
        m_batchTimer.restart();
    }
}
