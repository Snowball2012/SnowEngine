#include "StdAfx.h"

#include "UploadBufferPool.h"

void UploadBufferPool::Reset()
{
    if ( m_entries.size() > 1 )
    {
        // leave only one buffer with the capacity to hold everything
        size_t total_size = 0;
        for ( const auto& entry : m_entries )
        {
            total_size += entry.buffer->GetBuffer()->GetSize();
        }

        m_entries.clear();
        AllocateNewEntry( total_size );
    }
    else if ( m_entries.size() == 1 )
    {
        m_entries.front().current_offset = 0;
    }
}

UploadBufferRange UploadBufferPool::Allocate( uint64_t size )
{
    UploadBufferPoolEntry* entry = nullptr;
    if ( ! m_entries.empty() )
    {
        // try fit into last entry
        auto& last_entry = m_entries.back();
        uint64_t required_offset = CalcAlignedSize( last_entry.current_offset, GetRHI().GetUniformBufferMinAlignment() );

        uint64_t required_size = required_offset + size;

        if ( last_entry.buffer->GetBuffer()->GetSize() >= required_size )
        {
            last_entry.current_offset = required_offset;
            entry = &last_entry;
        }
    }

    if ( !entry )
    {
        entry = AllocateNewEntry( size );
    }

    UploadBufferRange retval = {};
    retval.buffer = entry->buffer.get();
    retval.view.buffer = retval.buffer->GetBuffer();
    retval.view.offset = entry->current_offset;
    retval.view.range = size;

    entry->current_offset += size;

    return retval;
}

UploadBufferPoolEntry* UploadBufferPool::AllocateNewEntry( uint64_t size )
{
    size = CalcAlignedSize( size, DefaultBlockSize );

    RHI::BufferInfo buf_info = {};
    buf_info.size = size;
    buf_info.usage = m_usage;
    auto& new_entry = m_entries.emplace_back();
    new_entry.buffer = GetRHI().CreateUploadBuffer( buf_info );
    new_entry.current_offset = 0;

    return &new_entry;
}
