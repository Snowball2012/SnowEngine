// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "stdafx.h"

#include "GPUCircularBuffers.h"


namespace
{
	
}


GPUMappedCircularBuffer::GPUMappedCircularBuffer(
    ComPtr<ID3D12Device> device, uint64_t size, uint64_t alignment,
    D3D12_HEAP_TYPE heap_type, D3D12_RESOURCE_STATES default_resource_state )
	: m_allocator( 
		device.Get(),
		CD3DX12_HEAP_DESC(
			CalcAlignedSize( size, alignment ), heap_type, UINT64( alignment ),
			D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS ) )
	, m_alignment( alignment )
    , m_default_state( default_resource_state )
{
	if ( alignment & ( alignment - 1 ) )
		throw SnowEngineException( "alignment must be a power of 2!" );

	m_heap = m_allocator.GetHeap();
	m_device = std::move( device );
}


std::pair<ID3D12Resource*, span<uint8_t>> GPUMappedCircularBuffer::AllocateBuffer( uint64_t size )
{
    size = CalcAlignedSize( size, m_alignment );

    auto res = std::make_pair<ID3D12Resource*, span<uint8_t>>( nullptr, span<uint8_t>() );

    auto mem_allocation = m_allocator.Alloc( size );
    if ( mem_allocation.first == nullptr )
        return res;

    m_allocations.emplace();
    auto& alloc = m_allocations.back();
    alloc.placed_res.Reset();
    alloc.offset_in_heap = mem_allocation.second;

    ThrowIfFailedH( m_device->CreatePlacedResource( m_heap,
        alloc.offset_in_heap,
        &CD3DX12_RESOURCE_DESC::Buffer( size ),
        m_default_state,
        nullptr,
        IID_PPV_ARGS( alloc.placed_res.GetAddressOf() ) ) );

    uint8_t* mapped_data;
    ThrowIfFailedH( alloc.placed_res->Map( 0, nullptr, reinterpret_cast<void**>( &mapped_data ) ) );

    alloc.mapped_data = make_span( mapped_data, mapped_data + size );

    res.first = alloc.placed_res.Get();
    res.second = alloc.mapped_data;
    return res;
}


void GPUMappedCircularBuffer::DeallocateOldest()
{
    if ( m_allocations.empty() )
        throw SnowEngineException( "buffer is already empty. Something strange happened in the caller function" );

    auto& alloc = m_allocations.front();

    alloc.placed_res->Unmap( 0, nullptr );
    m_allocations.pop();

    m_allocator.Free();
}


uint64_t GPUMappedCircularBuffer::GetFreeMem() const noexcept
{
    return m_allocator.GetMaxAllocSize();
}

GPUMappedCircularUploadBuffer::GPUMappedCircularUploadBuffer(
    ComPtr<ID3D12Device> device, uint64_t size, uint64_t alignment )
    : GPUMappedCircularBuffer( std::move( device ), size, alignment,
        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ )
{
}

GPUMappedCircularReadbackBuffer::GPUMappedCircularReadbackBuffer(
    ComPtr<ID3D12Device> device, uint64_t size, uint64_t alignment )
    : GPUMappedCircularBuffer( std::move( device ), size, alignment,
        D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COMMON ) // not sure about the COMMON state
{
}
