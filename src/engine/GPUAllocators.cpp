// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "stdafx.h"
#include "GPUAllocators.h"


// Linear allocator


GPULinearAllocator::GPULinearAllocator( ID3D12Device* device, const D3D12_HEAP_DESC& desc )
    : m_device( device )
    , m_cur_heap_offset( 0 )
    , m_cur_heap_idx( 0 )
    , m_initial_desc( desc )
{
    OPTICK_EVENT( );

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


GPUCircularAllocator::GPUCircularAllocator(
    ID3D12Device* device,
    const D3D12_HEAP_DESC& desc)
    : m_device( device )
    , m_initial_desc( desc )
{
    OPTICK_EVENT( );

    assert( m_device );

    ThrowIfFailedH( m_device->CreateHeap( &desc, IID_PPV_ARGS( m_heap.GetAddressOf() ) ) );

    if ( m_heap )
        m_total_size = m_heap->GetDesc().SizeInBytes;
}


// Circular allocator


std::pair<ID3D12Heap*, uint64_t> GPUCircularAllocator::Alloc( uint64_t size )
{
    const std::pair<ID3D12Heap*, uint64_t> null_alloc { nullptr, 0 };

    size = CalcAllocationSize( size );
    uint64_t alloc_start = std::numeric_limits<uint64_t>::max();

    if ( m_tail > m_head )
    {
        const uint64_t available_space = m_tail - m_head;
        if ( available_space < size )
            return null_alloc;

        alloc_start = m_head;
    }
    else
    {
        // 2 potentially available regions, after the head and before the tail

        // check free space in front of the head first
        const uint64_t front_available_space = GetTotalMem() - m_head;
        if ( front_available_space >= size )
            alloc_start = m_head;
        else if ( m_tail >= size )
            alloc_start = 0;
        else
            return null_alloc;
    }

    return AllocAt( alloc_start, size );
}


void GPUCircularAllocator::Free()
{
    if ( !SE_ENSURE( !m_previous_heads.empty() ) ) // nothing left to free, most certainly an error in the calling code
        return;

    m_tail = m_previous_heads.front();
    m_previous_heads.pop();
    if ( m_previous_heads.empty() )
    {
        // this was the last allocation, reset the queue to avoid unnecessary fragmentation
        m_head = 0;
        m_tail = 0;
    }
}


uint64_t GPUCircularAllocator::GetMaxAllocSize() const
{
    if ( m_tail > m_head )
        return m_tail - m_head;

    return std::max<uint64_t>( m_tail, uint64_t( GetTotalMem() - m_head ) );
}


uint64_t GPUCircularAllocator::GetFreeMem() const
{
    if ( m_tail > m_head )
        return m_tail - m_head;

    return m_tail + uint64_t( GetTotalMem() - m_head );
}


uint64_t GPUCircularAllocator::GetTotalMem() const
{
    return m_total_size;
}


uint64_t GPUCircularAllocator::CalcAllocationSize( uint64_t size ) const
{
    const UINT64 alignment = std::max<UINT64>( D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT, m_initial_desc.Alignment ); // min 64k alignment
    return ceil_integer_div( size, alignment ) * alignment;
}


std::pair<ID3D12Heap*, uint64_t> GPUCircularAllocator::AllocAt( uint64_t alloc_start, uint64_t size )
{
    std::pair<ID3D12Heap*, uint64_t> retval { nullptr, 0 };

    retval.first = m_heap.Get();
    retval.second = alloc_start;
    m_previous_heads.push( m_head );
    m_head = alloc_start + size;

    return retval;
}
