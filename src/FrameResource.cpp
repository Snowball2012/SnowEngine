// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "FrameResource.h"

FrameResource::FrameResource( ID3D12Device* device, size_t cmd_allocator_count )
{
	cmd_list_allocs.resize( cmd_allocator_count );

	for ( auto& alloc : cmd_list_allocs )
		ThrowIfFailed( device->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS( alloc.GetAddressOf() ) ) );
}