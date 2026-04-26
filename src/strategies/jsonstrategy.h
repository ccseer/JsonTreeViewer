#pragma once

#include <simdjson.h>

#include <QFlags>
#include <QPair>
#include <QString>
#include <QVector>
#include <memory>

#include "../common.h"

class JsonTreeItem;

namespace StrategyThresholds {
constexpr qint64 SMALL_FILE_MAX  = 10 * 1024 * 1024;
constexpr qint64 MEDIUM_FILE_MAX = 100 * 1024 * 1024;
constexpr qint64 LARGE_FILE_MAX  = 1024 * 1024 * 1024LL;
}  // namespace StrategyThresholds

/**
 * @brief Base class for JSON viewing strategies
 *
 * This class defines the interface for different file size strategies.
 * All methods use metadata (pointer, offset, length) instead of UI object
 * pointers, making them safe to call from background threads.
 */
class JsonViewerStrategy {
public:
    enum class CopyAction {
        Key      = 1 << 0,
        Value    = 1 << 1,
        Path     = 1 << 2,
        KeyValue = 1 << 3,
        Subtree  = 1 << 4,
    };
    Q_DECLARE_FLAGS(CopyActions, CopyAction)

    /**
     * @brief Factory method to create appropriate strategy based on file size
     * @param fileSize Size of the JSON file in bytes
     * @return Shared pointer to the created strategy
     */
    static std::shared_ptr<JsonViewerStrategy> createStrategy(qint64 fileSize);

    virtual ~JsonViewerStrategy() = default;

    /**
     * @brief Get the type of this strategy
     * @return Strategy type identifier
     */
    virtual FileMode type() const = 0;

    /**
     * @brief Initialize the strategy by loading the file
     * @param path Path to the JSON file
     * @return true on success, false on failure
     *
     * This method reads the file content and prepares the strategy for use.
     * It can be called from a background thread.
     */
    virtual bool initialize(const QString& path) = 0;

    /**
     * @brief Get metadata for the root node
     * @param[out] pointer JSON pointer for the root (usually "/")
     * @param[out] byte_offset Byte offset in file
     * @param[out] byte_length Byte length of the root value
     * @param[out] child_count Number of children
     *
     * This method can be called from a background thread.
     */
    virtual void getRootMetadata(QString& pointer,
                                 quint64& byte_offset,
                                 quint64& byte_length,
                                 quint32& child_count)
        = 0;

    /**
     * @brief Extract children using metadata
     * @param parent_pointer JSON pointer of the parent
     * @param byte_offset Byte offset of the parent value in file
     * @param byte_length Byte length of the parent value
     * @param start Start index (inclusive, -1 for all)
     * @param end End index (inclusive, -1 for all)
     * @return Vector of child items (caller takes ownership)
     *
     * This method uses metadata instead of UI object pointers, making it
     * safe to call from background threads. It should check for thread
     * interruption periodically during heavy loops.
     */
    struct Metrics {
        QString parseError;
        quint64 errorOffset = 0;
        QString errorContext;
        qint64 totalBytes  = 0;
        qint64 mappedBytes = 0;
        qint64 elapsedMs   = 0;
        QString strategyName;
    };

    struct CountResult {
        quint32 count = 0;
        QString error;
        quint64 offset = 0;
    };

    virtual const Metrics& metrics() const = 0;

    /**
     * @brief Extract children from the JSON file
     * @param parent_pointer JSON pointer of the parent
     * @param byte_offset Byte offset of the parent value in file
     * @param byte_length Byte length of the parent value
     * @param start Index of the first child to extract (-1 for all)
     * @param end Index of the last child to extract (-1 for all)
     * @return Vector of JsonTreeItem pointers
     */
    virtual QVector<JsonTreeItem*> extractChildren(
        const QString& parent_pointer,
        quint64 byte_offset,
        quint64 byte_length,
        int start = -1,
        int end   = -1)
        = 0;

    /**
     * @brief Count children using metadata
     * @param parent_pointer JSON pointer of the parent
     * @param byte_offset Byte offset of the parent value in file
     * @param byte_length Byte length of the parent value
     * @return CountResult containing child count and any error
     */
    virtual CountResult countChildren(const QString& parent_pointer,
                                      quint64 byte_offset,
                                      quint64 byte_length)
        = 0;

    virtual const char* dataPtr() const = 0;
    virtual size_t dataSize() const     = 0;

    virtual CopyActions supportedActions() const = 0;

protected:
    // Helper for strategies to count children in a local buffer
    static CountResult countLocalBufferChildren(const char* base_ptr,
                                                size_t base_size);

    // Helper to count children at a JSON pointer
    static CountResult countChildrenAtPointer(const QString& parent_pointer,
                                              const char* base_ptr,
                                              size_t base_size);

    // Helper to extract error context (nearby text)
    static QString extractErrorContext(const char* base_ptr,
                                       size_t base_size,
                                       quint64 offset);

    /**
     * @brief Parse children from a local buffer
     * @param parent_pointer JSON pointer of the parent
     * @param base_ptr Pointer to the JSON value
     * @param base_size Size of the JSON value
     * @param start Start index (inclusive, -1 for all)
     * @param end End index (inclusive, -1 for all)
     * @return Vector of child items (caller takes ownership)
     *
     * Helper method for parsing children from a memory buffer.
     * Should check for thread interruption periodically.
     */
    static QVector<JsonTreeItem*> parseLocalBuffer(
        const QString& parent_pointer,
        const char* base_ptr,
        size_t base_size,
        int start = -1,
        int end   = -1);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(JsonViewerStrategy::CopyActions)
