#pragma once

#include <windows.h>

#include <QString>

// Memory-mapped file wrapper for large files
class MemoryMappedFile {
public:
    MemoryMappedFile();
    ~MemoryMappedFile();

    bool map(const QString& path);
    void unmap();

    const char* data() const
    {
        return m_data;
    }
    size_t size() const
    {
        return m_size;
    }
    bool isMapped() const
    {
        return m_data != nullptr;
    }

private:
    HANDLE m_file_handle = INVALID_HANDLE_VALUE;
    HANDLE m_map_handle  = NULL;
    const char* m_data   = nullptr;
    size_t m_size        = 0;
};
