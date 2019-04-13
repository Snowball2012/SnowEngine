#pragma once

#include "span.h"

class MemoryMappedFile
{
public:
    MemoryMappedFile() = default;
    ~MemoryMappedFile();
    MemoryMappedFile( const MemoryMappedFile& other ) = delete;
    MemoryMappedFile( MemoryMappedFile&& other ) noexcept;
    MemoryMappedFile& operator=( MemoryMappedFile&& other ) noexcept;
    bool Open( const std::string_view& path ) noexcept;
    bool IsOpened() const noexcept;
    void Close() noexcept;
    const span<const uint8_t>& GetData() const noexcept { return m_mapped_file_data; }

private:
    HANDLE m_file_handle = INVALID_HANDLE_VALUE;
    HANDLE m_file_mapping = nullptr; // different default values because of different error values for corresponding winapi functions
    span<const uint8_t> m_mapped_file_data;
};