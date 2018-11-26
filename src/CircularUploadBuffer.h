#pragma once

#include "Ptr.h"

#include "utils/span.h"

#include <d3d12.h>
#include <queue>

// FIFO allocator with constant storage
class CircularUploadBuffer
{
public:
	// alignment must be power of 2
	CircularUploadBuffer( ComPtr<ID3D12Device> device, uint64_t size, uint64_t alignment );

	// returns nullptr if requested allocation is too big to fit in the heap
	std::pair<ID3D12Resource*, span<uint8_t>> AllocateBuffer( uint64_t size );
	void DeallocateOldest();

	ID3D12Heap* GetHeap() noexcept { return m_heap.Get(); }
	uint64_t GetFreeMem() const noexcept;

private:
	uint64_t CalcAlignedSize( uint64_t size ) const noexcept;

	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12Heap> m_heap;
	const uint64_t m_alignment;
	struct Allocation
	{
		ComPtr<ID3D12Resource> placed_res;
		span<uint8_t> mapped_data;
		UINT64 offset_in_heap;
	};
	std::queue<Allocation> m_allocations;

	UINT64 m_allocation_offset = 0;
};