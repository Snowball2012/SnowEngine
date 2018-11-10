#include "stdafx.h"

#include "FrameResource.h"

FrameResource::FrameResource( ID3D12Device* device, size_t cmd_allocator_count )
{
	cmd_list_allocs.resize( cmd_allocator_count );

	for ( auto& alloc : cmd_list_allocs )
		ThrowIfFailed( device->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS( alloc.GetAddressOf() ) ) );
}