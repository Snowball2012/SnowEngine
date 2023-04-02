#pragma once

#include "Ptr.h"

#include "GPUAllocators.h"

#include <utils/span.h>

#include <d3d12.h>
#include <queue>


// FIFO circular buffer with a constant storage
// Always mapped to a CPU region
class GPUMappedCircularBuffer
{
public:
    // alignment must be a power of 2
    GPUMappedCircularBuffer( 
        ComPtr<ID3D12Device> device, uint64_t size, uint64_t alignment,
        D3D12_HEAP_TYPE heap_type, D3D12_RESOURCE_STATES default_resource_state );

    // returns nullptr if requested allocation is too big to fit in the heap
    std::pair<ID3D12Resource*, span<uint8_t>> AllocateBuffer( uint64_t size );
    void DeallocateOldest();

    ID3D12Heap* GetHeap() noexcept { return m_heap; }
    uint64_t GetFreeMem() const noexcept;

private:

    GPUCircularAllocator m_allocator;

    ComPtr<ID3D12Device> m_device;

    ID3D12Heap* m_heap = nullptr;

    const uint64_t m_alignment = 0;
    struct Allocation
    {
        ComPtr<ID3D12Resource> placed_res;
        span<uint8_t> mapped_data;
        UINT64 offset_in_heap;
    };
    std::queue<Allocation> m_allocations;

    const D3D12_RESOURCE_STATES m_default_state = D3D12_RESOURCE_STATE_COMMON;

    UINT64 m_allocation_offset = 0;
};


class GPUMappedCircularUploadBuffer : public GPUMappedCircularBuffer
{
public:
    GPUMappedCircularUploadBuffer(
        ComPtr<ID3D12Device> device, uint64_t size, uint64_t alignment );
};


class GPUMappedCircularReadbackBuffer : public GPUMappedCircularBuffer
{
public:
    GPUMappedCircularReadbackBuffer(
        ComPtr<ID3D12Device> device, uint64_t size, uint64_t alignment );
};
