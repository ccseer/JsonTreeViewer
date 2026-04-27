#include "searchworker.h"

#include <QDebug>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QThread>

SearchWorker::SearchWorker(std::shared_ptr<JsonViewerStrategy> strategy,
                           const SearchQuery& query,
                           QObject* parent)
    : QObject(parent), m_strategy(strategy), m_query(query)
{
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
            qDebug() << "SearchWorker: Invalid regex:" << m_re.errorString();
        }
    }
}

void SearchWorker::process()
{
    // Chained cleanup trigger
    QScopeGuard _cleanup([this]() { this->deleteLater(); });

    if (!m_data || m_size == 0) {
        emit finished(false);
        return;
    }

    if (m_query.useRegex && !m_re.isValid()) {
        emit finished(false);
        return;
    }

    simdjson::ondemand::parser parser;

    try {
        // NO COPY: Use m_data directly as the strategy guarantees it's padded.
        simdjson::ondemand::document doc;
        auto error
            = parser
                  .iterate(m_data, m_size, m_size + simdjson::SIMDJSON_PADDING)
                  .get(doc);
        if (error) {
            qDebug() << "SearchWorker: Simdjson error:"
                     << simdjson::error_message(error);
            emit finished(false);
            return;
        }

        m_batchTimer.start();
        m_totalResults = 0;
        m_lastProgress = -1;

        simdjson::ondemand::value val;
        if (doc.get_value().get(val) == simdjson::SUCCESS) {
            searchRecursive(val, "", "", m_data);
        }

        emitBatch(true);
        emit finished(true);
    }
    catch (const simdjson::simdjson_error& e) {
        qDebug() << "SearchWorker: simdjson error:" << e.what();
        emit finished(false);
    }
    catch (const std::exception& e) {
        qDebug() << "SearchWorker: Exception:" << e.what();
        emit finished(false);
    }
    catch (...) {
        qDebug() << "SearchWorker: Unknown exception";
        emit finished(false);
    }
}

void SearchWorker::searchRecursive(simdjson::ondemand::value val,
                                   QString currentPath,
                                   const QString& currentKey,
                                   const char* basePtr)
{
    if (QThread::currentThread()->isInterruptionRequested())
        return;
    if (m_totalResults >= MAX_RESULTS)
        return;

    simdjson::ondemand::json_type type;
    if (val.type().get(type) != simdjson::SUCCESS)
        return;

    std::string_view tok = val.raw_json_token();
    if (!tok.empty()) {
        const char* currentPtr = tok.data();
        if (currentPtr >= basePtr && currentPtr < basePtr + m_size) {
            int progress
                = static_cast<int>((currentPtr - basePtr) * 100 / m_size);
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
            emitBatch();
        }
    }

    if (!isContainer
        && (m_query.type == SearchType::Value
            || m_query.type == SearchType::All)) {
        if (matches(valueSnippet)) {
            m_batch.append({currentKey, valueSnippet, currentPath, typeChar});
            m_totalResults++;
            emitBatch();
        }
    }

    if (m_query.type == SearchType::Path && matches(currentPath)) {
        m_batch.append({currentKey, valueSnippet, currentPath, typeChar});
        m_totalResults++;
        emitBatch();
    }

    if (m_totalResults >= MAX_RESULTS)
        return;

    if (type == simdjson::ondemand::json_type::object) {
        simdjson::ondemand::object obj;
        if (val.get_object().get(obj) == simdjson::SUCCESS) {
            for (auto field : obj) {
                if (QThread::currentThread()->isInterruptionRequested())
                    return;
                std::string_view key_view;
                if (field.unescaped_key().get(key_view) == simdjson::SUCCESS) {
                    QString key
                        = QString::fromUtf8(key_view.data(), key_view.size());
                    QString escapedKey = key;
                    escapedKey.replace("~", "~0").replace("/", "~1");
                    QString nextPath = currentPath + "/" + escapedKey;
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
                if (QThread::currentThread()->isInterruptionRequested())
                    return;
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
    Qt::CaseSensitivity cs
        = m_query.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    return text.contains(m_query.text, cs);
}

void SearchWorker::emitBatch(bool force)
{
    if (m_batch.isEmpty())
        return;
    if (force || m_batch.size() >= 50 || m_batchTimer.elapsed() >= 100) {
        emit resultsFound(m_batch);
        m_batch.clear();
        m_batchTimer.restart();
    }
}
