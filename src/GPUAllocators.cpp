// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "stdafx.h"
#include "GPUAllocators.h"

GPULinearAllocator::GPULinearAllocator( ID3D12Device* device, const D3D12_HEAP_DESC& desc )
    : m_device( device )
    , m_cur_heap_offset( 0 )
    , m_cur_heap_idx( 0 )
    , m_initial_desc( desc )
{
    assert( m_device );

    m_heaps.emplace_back();
    ThrowIfFailedH( m_device->CreateHeap( &desc, IID_PPV_ARGS( m_heaps.back().GetAddressOf() ) ) );
}


void GPULinearAllocator::Reserve( uint64_t size )
{
    size = CalcAllocationSize( size );
    uint64_t free_mem = GetFreeMem();
    if ( free_mem < size )
        AllocateAdditionalHeap( size );
}


std::pair<ID3D12Heap*, uint64_t> GPULinearAllocator::Alloc( uint64_t size )
{
    size = CalcAllocationSize( size );
    uint64_t free_mem = GetFreeMem();
    if ( free_mem < size )
        AllocateAdditionalHeap( size );

    std::pair<ID3D12Heap*, uint64_t> retval;
    if ( m_heaps[m_cur_heap_idx]->GetDesc().SizeInBytes - m_cur_heap_offset >= size )
    {
        retval.first = m_heaps[m_cur_heap_idx].Get();
    }
    else
    {
        retval.first = m_heaps.back().Get();
        m_cur_heap_offset = 0;
        m_cur_heap_idx++;
    }

    retval.second = m_cur_heap_offset;
    m_cur_heap_offset += size;

    return retval;
}


void GPULinearAllocator::Shrink()
{
    if ( m_heaps.size() > m_cur_heap_idx + 1 )
        m_heaps.resize( m_cur_heap_idx + 1 );
}


uint64_t GPULinearAllocator::GetFreeMem() const
{
    if ( m_cur_heap_idx + 1 < m_heaps.size() )
        return m_heaps.back()->GetDesc().SizeInBytes;

    return m_heaps[m_cur_heap_idx]->GetDesc().SizeInBytes - m_cur_heap_offset;
}


uint64_t GPULinearAllocator::GetTotalMem() const
{
    uint64_t total_mem = 0;
    for ( const auto& heap : m_heaps )
        total_mem += heap->GetDesc().SizeInBytes;

    return total_mem;
}


void GPULinearAllocator::AllocateAdditionalHeap( uint64_t size )
{
    D3D12_HEAP_DESC new_heap_desc = m_initial_desc;
    new_heap_desc.SizeInBytes = std::max( CalcAllocationSize( size ),
                                          m_initial_desc.SizeInBytes );

    if ( m_cur_heap_idx + 1 == m_heaps.size() )
        m_heaps.emplace_back();
    else
        m_heaps.resize( m_cur_heap_idx + 1 );
    
    ThrowIfFailedH( m_device->CreateHeap( &new_heap_desc, IID_PPV_ARGS( m_heaps.back().GetAddressOf() ) ) );
}


uint64_t GPULinearAllocator::CalcAllocationSize( uint64_t size ) const
{
    const UINT64 alignment = std::max<UINT64>( 1 << 16, m_initial_desc.Alignment ); // min 64k alignment
    return ceil_integer_div( size, alignment ) * alignment;
}


