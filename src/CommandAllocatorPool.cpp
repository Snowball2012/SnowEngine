// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "CommandAllocatorPool.h"


CommandAllocator::CommandAllocator( ID3D12CommandAllocator* allocator, D3D12_COMMAND_LIST_TYPE type ) noexcept
	: m_allocator( allocator ), m_type( type )
{
	assert( m_allocator );
}


ID3D12CommandAllocator* CommandAllocator::GetAllocator() noexcept
{
	return m_allocator;
}


const ID3D12CommandAllocator* CommandAllocator::GetAllocator() const noexcept
{
	return m_allocator;
}


HRESULT CommandAllocator::Reset() noexcept
{
	return m_allocator->Reset();
}


D3D12_COMMAND_LIST_TYPE CommandAllocator::GetType() const noexcept
{
	return m_type;
}


CommandAllocatorPool::CommandAllocatorPool( ID3D12Device* device ) noexcept
	: m_device( device )
{
	assert( m_device );
}


CommandAllocator CommandAllocatorPool::Allocate( D3D12_COMMAND_LIST_TYPE type )
{
	auto& pool = m_pools[type];

	auto& free_allocators = pool.free_allocators;
	if ( ! free_allocators.empty() )
	{
		auto* allocator = free_allocators.back();
		free_allocators.pop_back();

		ThrowIfFailed( allocator->Reset() );
		return CommandAllocator( allocator, type );
	}

	auto& storage = pool.storage;
	storage.emplace_back();
	ThrowIfFailed( m_device->CreateCommandAllocator( type, IID_PPV_ARGS( storage.back().GetAddressOf() ) ) );

	return CommandAllocator( storage.back().Get(), type );
}


void CommandAllocatorPool::Deallocate( CommandAllocator&& allocator )
{
	m_pools[allocator.GetType()].free_allocators.push_back( allocator.GetAllocator() );
}
