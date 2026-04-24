#include "memorymappedfile.h"

#include "logging.h"

#define qprintt qprint << "[MemoryMappedFile]"

MemoryMappedFile::MemoryMappedFile() = default;

MemoryMappedFile::~MemoryMappedFile()
{
    unmap();
}

bool MemoryMappedFile::map(const QString& path)
{
    unmap();

    m_file_handle = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()),
                                GENERIC_READ, FILE_SHARE_READ, NULL,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (m_file_handle == INVALID_HANDLE_VALUE) {
        qprintt << "Failed to open file for memory mapping:" << path;
        return false;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(m_file_handle, &fileSize)) {
        qprintt << "Failed to get file size for memory mapping";
        CloseHandle(m_file_handle);
        m_file_handle = INVALID_HANDLE_VALUE;
        return false;
    }

    m_size = static_cast<size_t>(fileSize.QuadPart);

    m_map_handle
        = CreateFileMappingW(m_file_handle, NULL, PAGE_READONLY, 0, 0, NULL);

    if (m_map_handle == NULL) {
        qprintt << "Failed to create file mapping";
        CloseHandle(m_file_handle);
        m_file_handle = INVALID_HANDLE_VALUE;
        return false;
    }

    m_data = static_cast<const char*>(
        MapViewOfFile(m_map_handle, FILE_MAP_READ, 0, 0, 0));

    if (m_data == nullptr) {
        qprintt << "Failed to map view of file";
        CloseHandle(m_map_handle);
        CloseHandle(m_file_handle);
        m_map_handle  = NULL;
        m_file_handle = INVALID_HANDLE_VALUE;
        return false;
    }

    qprintt << "Successfully memory-mapped file:" << path << "Size:" << m_size;
    return true;
}

void MemoryMappedFile::unmap()
{
    if (m_data != nullptr) {
        UnmapViewOfFile(m_data);
        m_data = nullptr;
    }
    if (m_map_handle != NULL) {
        CloseHandle(m_map_handle);
        m_map_handle = NULL;
    }
    if (m_file_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_file_handle);
        m_file_handle = INVALID_HANDLE_VALUE;
    }
    m_size = 0;
}
