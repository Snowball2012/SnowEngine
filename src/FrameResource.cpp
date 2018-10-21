#include "stdafx.h"

#include "FrameResource.h"

FrameResource::FrameResource( ID3D12Device* device, size_t cmd_allocator_count, UINT passCount, UINT dynamic_vertices_cnt )
{
	cmd_list_allocs.resize( cmd_allocator_count );

	for ( auto& alloc : cmd_list_allocs )
		ThrowIfFailed( device->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS( alloc.GetAddressOf() ) ) );
	if ( passCount > 0 )
		pass_cb = std::make_unique<Utils::UploadBuffer<PassConstants>>( device, passCount, true );
	if ( dynamic_vertices_cnt > 0 )
		dynamic_geom_vb = std::make_unique<Utils::UploadBuffer<Vertex>>( device, dynamic_vertices_cnt, false );
}