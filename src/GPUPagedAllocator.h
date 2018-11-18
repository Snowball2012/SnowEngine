#pragma once

#include "Ptr.h"
#include "utils/packed_freelist.h"

#include <d3d12.h>

class GPUPagedAllocator
{
	using Chunk = std::vector<uint32_t>;
public:
	GPUPagedAllocator( ComPtr<ID3D12Heap> heap );

	using ChunkID = packed_freelist<Chunk>::id;

	// returns nullid on failed allocation
	ChunkID Alloc( uint32_t npages );
	void Free( ChunkID id ) noexcept;

	// returns empty range if there is no chunk with this id
	span<const uint32_t> GetPages( ChunkID id ) const noexcept;
	uint32_t GetFreePagesNum() const noexcept { return m_free_pages_num; }
	ID3D12Heap* GetDXHeap() noexcept { return m_heap.Get(); }

	static constexpr uint32_t PageSize = 1 << 16;

private:

	uint32_t m_free_pages_num = 0;
	ComPtr<ID3D12Heap> m_heap;
	packed_freelist<Chunk> m_allocated_chunks;
	std::vector<Chunk> m_free_chunks;

	// for std::vector memory reuse
	std::vector<Chunk> m_free_chunk_vectors;
};