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


// Circular buffer with constant size

class GPUCircularAllocator
{
public:
    GPUCircularAllocator( ID3D12Device* device, uint64_t size, const D3D12_HEAP_DESC& desc );

    GPUCircularAllocator( const GPUCircularAllocator& ) = delete;
    GPUCircularAllocator( GPUCircularAllocator&& ) = default;
    GPUCircularAllocator& operator=( GPUCircularAllocator&& ) = default;


    std::pair<ID3D12Heap*, uint64_t> Alloc( uint64_t size );

    // Frees the oldest allocation alive
    void Free();

    // Returns a maximum amount of memory we can allocate in a single call. 
    uint64_t GetMaxAllocSize() const;

    // Returns a maximum chunk of memory we can return right now. 
    uint64_t GetFreeMem() const;

    uint64_t GetTotalMem() const;

private:
    ComPtr<ID3D12Heap> m_heap;
    uint64_t m_total_size = 0;
    uint64_t m_head = 0;
    uint64_t m_tail = 0;
    std::queue<uint64_t> m_previous_heads;
    D3D12_HEAP_DESC m_initial_desc;

    ID3D12Device* m_device = nullptr;

private:
    uint64_t CalcAllocationSize( uint64_t size ) const;

    // does not check if the segment is actually available
    std::pair<ID3D12Heap*, uint64_t> AllocAt( uint64_t alloc_start, uint64_t size );

};