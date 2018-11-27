// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "StagingDescriptorHeap.h"

StagingDescriptorHeap::StagingDescriptorHeap( D3D12_DESCRIPTOR_HEAP_TYPE type, Microsoft::WRL::ComPtr<ID3D12Device> device )
	: m_device( std::move( device ) ), m_type( type )
{
	AddChunk();
}

Descriptor StagingDescriptorHeap::AllocateDescriptor()
{
	for ( auto& chunk : m_chunks )
	{
		if ( chunk.GetSpareDescriptorsCount() > 0 )
			return std::move( chunk.AllocateDescriptor() );
	}

	// all chunks are full, create new
	AddChunk();
	return std::move( m_chunks.back().AllocateDescriptor() );
}

void StagingDescriptorHeap::AddChunk()
{
	const UINT desc_size = m_device->GetDescriptorHandleIncrementSize( m_type );

	D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};

	heap_desc.NumDescriptors = DefaultChunkSizeInDescriptors;
	heap_desc.Type = m_type;
	heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	ComPtr<ID3D12DescriptorHeap> heap;
	ThrowIfFailed( m_device->CreateDescriptorHeap( &heap_desc, IID_PPV_ARGS( heap.GetAddressOf() ) ) );
	
	m_chunks.emplace_back( std::move( heap ), desc_size );
}
