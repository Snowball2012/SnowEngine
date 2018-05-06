#include "stdafx.h"

#include "FrameResource.h"

FrameResource::FrameResource( ID3D12Device* device, UINT passCount, UINT objectCount, UINT dynamic_vertices_cnt )
{
	ThrowIfFailed( device->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS( cmd_list_alloc.GetAddressOf() ) ) );
	if ( passCount > 0 )
		pass_cb = std::make_unique<Utils::UploadBuffer<PassConstants>>( device, passCount, true );
	if ( objectCount > 0 )
		object_cb = std::make_unique<Utils::UploadBuffer<ObjectConstants>>( device, objectCount, true );
	if ( dynamic_vertices_cnt > 0 )
		dynamic_geom_vb = std::make_unique<Utils::UploadBuffer<Vertex>>( device, dynamic_vertices_cnt, false );
}