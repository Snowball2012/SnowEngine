#pragma once

#include "Ptr.h"


class CommandAllocator
{
public:
	CommandAllocator( ID3D12CommandAllocator* allocator, D3D12_COMMAND_LIST_TYPE type ) noexcept;

	ID3D12CommandAllocator* GetAllocator() noexcept;
	const ID3D12CommandAllocator* GetAllocator() const noexcept;
	HRESULT Reset() noexcept;
	D3D12_COMMAND_LIST_TYPE GetType() const noexcept;

private:
	ID3D12CommandAllocator* m_allocator;
	D3D12_COMMAND_LIST_TYPE m_type;
};


class CommandAllocatorPool
{
public:
	CommandAllocatorPool( ID3D12Device* device ) noexcept;

	CommandAllocator Allocate( D3D12_COMMAND_LIST_TYPE type );
	void Deallocate( CommandAllocator&& allocator );

private:
	static constexpr uint32_t NumAllocatorTypes = 6; // see D3D12_COMMAND_LIST_TYPE enum in d3d12.h

	struct
	{
		std::deque<ComPtr<ID3D12CommandAllocator>> storage;
		std::vector<ID3D12CommandAllocator*> free_allocators;
	} m_pools[NumAllocatorTypes];

	ID3D12Device* m_device;
};