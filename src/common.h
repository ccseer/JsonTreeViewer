#pragma once

#include <QDebug>
#include <QThread>

// Logging helper
#define qprint qDebug()
#define qprint_err qDebug() << "ERROR:" << __FILE__ << __LINE__

// Common enums and types used across the application
enum class FileMode { Small = 0, Medium = 1, Large = 2, Extreme = 3 };

/**
 * @brief Custom QThread that automatically cleans up in destructor
 *
 * This class ensures that quit() and wait() are called in the destructor.
 * The thread will finish cleanly without blocking indefinitely.
 *
 * IMPORTANT: Always use deleteLater() instead of direct delete to ensure
 * proper cleanup through Qt's event loop.
 */
class JTVThread : public QThread {
    Q_OBJECT

    // Hide run() to prevent direct override - use worker pattern instead
    using QThread::run;

public:
    explicit JTVThread(QObject* parent = nullptr) : QThread(parent) {}

    ~JTVThread() override
    {
        quit();
        wait();
    }
};
