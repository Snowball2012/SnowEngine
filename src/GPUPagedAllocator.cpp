#include "stdafx.h"

#include "GPUPagedAllocator.h"


GPUPagedAllocator::GPUPagedAllocator( ComPtr<ID3D12Heap> heap )
	: m_heap( std::move( heap ) )
{
	m_free_chunks.emplace_back();
	auto& free_chunk = m_free_chunks.back();

	const D3D12_HEAP_DESC& desc = m_heap->GetDesc();
	m_free_pages_num = ( desc.SizeInBytes + PageSize - 1 ) / PageSize;
	free_chunk.resize( m_free_pages_num );
	for ( uint32_t page = 0; page < m_free_pages_num; ++page )
		free_chunk[page] = page;
}


GPUPagedAllocator::ChunkID GPUPagedAllocator::Alloc( uint32_t npages )
{
	if ( npages > m_free_pages_num )
		return ChunkID::nullid;

	m_free_pages_num -= npages;

	if ( m_free_chunks.empty() )
		throw SnowEngineException( "gpu heap corruption" );

	Chunk new_chunk = std::move( m_free_chunks.back() );
	m_free_chunks.pop_back();

	if ( new_chunk.size() > npages )
	{
		if ( m_free_chunk_vectors.empty() )
			m_free_chunk_vectors.emplace_back();
		Chunk free_chunk = std::move( m_free_chunk_vectors.back() );
		m_free_chunk_vectors.pop_back();

		free_chunk.resize( new_chunk.size() - npages );
		free_chunk.assign( new_chunk.cbegin() + npages, new_chunk.cend() );

		new_chunk.resize( npages );

		m_free_chunks.emplace_back( std::move( free_chunk ) );
	}
	else if ( new_chunk.size() < npages )
	{
		while ( ! m_free_chunks.empty() && new_chunk.size() < npages )
		{
			const uint32_t pages_left = npages - uint32_t( new_chunk.size() );

			auto& free_chunk = m_free_chunks.back();
			if ( free_chunk.size() > pages_left )
			{
				new_chunk.insert( new_chunk.end(), free_chunk.cend() - pages_left, free_chunk.cend() );
				free_chunk.resize( free_chunk.size() - pages_left );
			}
			else
			{
				new_chunk.insert( new_chunk.end(), free_chunk.cbegin(), free_chunk.cend() );
				free_chunk.clear();
				m_free_chunk_vectors.emplace_back( std::move( free_chunk ) );
				m_free_chunks.pop_back();
			}
		}
		if ( new_chunk.size() != npages )
			throw SnowEngineException( "gpu heap corruption" );
	}

	return m_allocated_chunks.emplace( std::move( new_chunk ) );
}


void GPUPagedAllocator::Free( ChunkID id ) noexcept
{
	Chunk* chunk = m_allocated_chunks.try_get( id );
	if ( ! chunk ) // already freed
		return;

	m_free_pages_num += uint32_t( chunk->size() );

	m_free_chunks.emplace_back( std::move( *chunk ) );
}


span<const uint32_t> GPUPagedAllocator::GetPages( ChunkID id ) const noexcept
{
	const Chunk* chunk = m_allocated_chunks.try_get( id );

	if ( ! chunk )
		return span<const uint32_t>();

	return make_span( chunk->data(), chunk->data() + chunk->size() );
}
