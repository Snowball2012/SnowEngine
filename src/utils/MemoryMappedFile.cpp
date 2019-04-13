// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "../stdafx.h"

#include "MemoryMappedFile.h"


MemoryMappedFile::~MemoryMappedFile()
{
    Close();
}


MemoryMappedFile::MemoryMappedFile( MemoryMappedFile&& other ) noexcept
{
    m_file_handle = other.m_file_handle;
    m_file_mapping = other.m_file_mapping;
    m_mapped_file_data = other.m_mapped_file_data;
    other.m_file_handle = INVALID_HANDLE_VALUE;
    other.m_file_mapping = nullptr;
    other.m_mapped_file_data = span<const uint8_t>();
}


MemoryMappedFile& MemoryMappedFile::operator=( MemoryMappedFile && other ) noexcept
{
    m_file_handle = other.m_file_handle;
    m_file_mapping = other.m_file_mapping;
    m_mapped_file_data = other.m_mapped_file_data;
    other.m_file_handle = INVALID_HANDLE_VALUE;
    other.m_file_mapping = nullptr;
    other.m_mapped_file_data = span<const uint8_t>();
    return *this;
}


bool MemoryMappedFile::Open( const std::string_view& path ) noexcept
{
    // be careful with return value for failed winapi functions!
    // different functions return different values for handles!
    if ( IsOpened() )
        return false;

    m_file_handle = CreateFileA( path.data(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
    if ( m_file_handle == INVALID_HANDLE_VALUE )
        return false;

    LARGE_INTEGER filesize;
    if ( ! GetFileSizeEx( m_file_handle, &filesize ) )
        return false;
    if ( filesize.QuadPart == 0 )
        return false;
    m_file_mapping = CreateFileMappingA( m_file_handle, nullptr, PAGE_READONLY, 0, 0, NULL );
    if ( ! m_file_mapping )
        return false;

    const uint8_t* mapped_region_start = reinterpret_cast<const uint8_t*>( MapViewOfFile( m_file_mapping, FILE_MAP_READ, 0, 0, 0 ) );
    if ( ! mapped_region_start )
        return false;

    m_mapped_file_data = span<const uint8_t>( mapped_region_start, mapped_region_start + filesize.QuadPart );

    return true;
}


bool MemoryMappedFile::IsOpened() const noexcept
{
    return m_file_handle != nullptr && m_file_handle != INVALID_HANDLE_VALUE
        && m_file_mapping != nullptr && m_file_mapping != INVALID_HANDLE_VALUE
        && m_mapped_file_data.cbegin() != nullptr;
}


void MemoryMappedFile::Close() noexcept
{
    if ( m_mapped_file_data.cbegin() != nullptr )
        UnmapViewOfFile( m_mapped_file_data.cbegin() );

    if ( m_file_mapping != nullptr && m_file_mapping != INVALID_HANDLE_VALUE )
        CloseHandle( m_file_mapping );

    if ( m_file_handle != nullptr && m_file_handle != INVALID_HANDLE_VALUE )
        CloseHandle( m_file_handle );
}
