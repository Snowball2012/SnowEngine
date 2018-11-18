#include "stdafx.h"

#include "CircularUploadBuffer.h"


CircularUploadBuffer::CircularUploadBuffer( ComPtr<ID3D12Device> device, uint64_t size, uint64_t alignment )
	: m_device( std::move( device ) ), m_alignment( alignment )
{
	if ( alignment & ( alignment - 1 ) )
		throw SnowEngineException( "alignment must be a power of 2!" );

	CD3DX12_HEAP_DESC heap_desc( CalcAlignedSize( size ), CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_UPLOAD ) );
	heap_desc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
	heap_desc.Alignment = alignment;

	ThrowIfFailed( m_device->CreateHeap( &heap_desc, IID_PPV_ARGS( m_heap.GetAddressOf() ) ) );	
}


std::pair<ID3D12Resource*, span<uint8_t>> CircularUploadBuffer::AllocateBuffer( uint64_t size )
{
	size = CalcAlignedSize( size );

	auto res = std::make_pair<ID3D12Resource*, span<uint8_t>>( nullptr, span<uint8_t>() );
	const uint64_t heap_size = m_heap->GetDesc().SizeInBytes;
	if ( size > heap_size )
		return res;

	uint64_t free_segment_end = size;

	if ( ! m_allocations.empty() )
		free_segment_end = m_allocations.back().offset_in_heap;

	uint64_t new_allocation_offset = m_allocation_offset;
	if ( free_segment_end >= m_allocation_offset )
	{
		if ( free_segment_end - m_allocation_offset < size )
			return res;
	}
	else
	{
		if ( heap_size - m_allocation_offset < size )
			if ( free_segment_end < size )
		{
				return res;
			new_allocation_offset = 0;
		}
	}
	m_allocations.emplace();
	auto& alloc = m_allocations.front();
	alloc.placed_res.Reset();


	ThrowIfFailed( m_device->CreatePlacedResource( m_heap.Get(),
												   new_allocation_offset,
												   &CD3DX12_RESOURCE_DESC::Buffer( size ),
												   D3D12_RESOURCE_STATE_COMMON,
												   nullptr,
												   IID_PPV_ARGS( alloc.placed_res.GetAddressOf() ) ) );

	uint8_t* mapped_data;
	ThrowIfFailed( alloc.placed_res->Map( 0, nullptr, reinterpret_cast<void**>( &mapped_data ) ) );

	alloc.mapped_data = make_span( mapped_data, mapped_data + size );

	alloc.offset_in_heap = new_allocation_offset;

	m_allocation_offset = new_allocation_offset + size;

	res.first = alloc.placed_res.Get();
	res.second = alloc.mapped_data;
	return res;
}


void CircularUploadBuffer::DeallocateOldest()
{
	if ( m_allocations.empty() )
		throw SnowEngineException( "buffer is already empty. Something strange happened in the caller function" );
	
	auto& alloc = m_allocations.back();

	alloc.placed_res->Unmap( 0, nullptr );
	m_allocations.pop();
	if ( m_allocations.empty() )
		m_allocation_offset = 0;
}


uint64_t CircularUploadBuffer::CalcAlignedSize( uint64_t size ) const noexcept
{
	if ( m_alignment > 1 )
		return ( size + ( m_alignment - 1 ) ) & ~( m_alignment - 1 );
}

