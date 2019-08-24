#pragma once

#include "Ptr.h"

#include <d3d12.h>
#include <vector>

// Can only allocate memory, deallocates everything on destruction
// Different allocations may be placed in the same heap

class GPULinearAllocator
{
public:
    // SizeInBytes field in "desc" marks the initial heap size and a
    // minimum chunk size in case another heap is allocated.
    // It may later grow if more memory then anticipated was requested
    GPULinearAllocator( ID3D12Device* device, const D3D12_HEAP_DESC& desc );

    GPULinearAllocator( const GPULinearAllocator& ) = delete;
    GPULinearAllocator( GPULinearAllocator&& ) = default;
    GPULinearAllocator& operator=( GPULinearAllocator&& ) = default;

    void Reserve( uint64_t size );

    // The second field of the result is an allocation offset in a heap
    std::pair<ID3D12Heap*, uint64_t> Alloc( uint64_t size );

    // Drops unused heaps, mostly useless
    void Shrink();

    // Returns amount of memory we can return without allocating another heap
    uint64_t GetFreeMem() const;

    // Returns both allocated and to-be-allocated memory
    uint64_t GetTotalMem() const;

private:
    std::vector<ComPtr<ID3D12Heap>> m_heaps;
    uint64_t m_cur_heap_idx;
    uint64_t m_cur_heap_offset;
    D3D12_HEAP_DESC m_initial_desc;

    ID3D12Device* m_device;

    void AllocateAdditionalHeap( uint64_t size );
    uint64_t CalcAllocationSize( uint64_t size ) const;
};