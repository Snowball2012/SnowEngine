#pragma once

#include "StdAfx.h"

struct UploadBufferRange
{
    RHIUploadBuffer* buffer = nullptr;
    RHIBufferViewInfo view;

    template<typename T>
    bool UploadData( const T& data )
    {
        if ( !SE_ENSURE( sizeof( data ) <= view.range ) )
            return false;

        buffer->WriteBytes( &data, sizeof( data ), view.offset );
        return true;
    }

    template<typename T>
    bool UploadData( const T* data, size_t num_elems )
    {
        size_t total_size = sizeof( data[0] ) * num_elems;
        if ( !SE_ENSURE( total_size <= view.range) )
            return false;

        buffer->WriteBytes( data, total_size, view.offset );
        return true;
    }
};

struct UploadBufferPoolEntry
{
    RHIUploadBufferPtr buffer;
    uint64_t current_offset = 0;
};

// This class holds reusable descriptor sets. Not thread-safe. You must call Reset to be able to reuse descriptors.
// Descriptor sets allocated from the pool "expire" when you call Reset and must not be used after that. Sadly we don't have any checks that explicitly prevent that.
class UploadBufferPool
{
private:
    static constexpr uint64_t DefaultBlockSize = 64 * SizeKB;
    static constexpr uint64_t AllocationAlignment = 16;
    std::vector<UploadBufferPoolEntry> m_entries;
    RHIBufferUsageFlags m_usage = RHIBufferUsageFlags::None;

public:

    UploadBufferPool( RHIBufferUsageFlags usage ) : m_usage( usage ) { }

    void Reset();

    UploadBufferRange Allocate( uint64_t size );

private:
    UploadBufferPoolEntry* AllocateNewEntry( uint64_t size );
};