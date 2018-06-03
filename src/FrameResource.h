#pragma once

#include "RenderUtils.h"

#include "RenderData.h"
#include "SceneImporter.h"

struct FrameResource
{
	FrameResource( ID3D12Device* device, size_t cmd_allocator_count, UINT passCount, UINT objectCount, UINT dynamic_vertices_cnt );

	// We cannot reset the allocator until the GPU is done processing the commands.
	// So each frame needs their own allocator.
	std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> cmd_list_allocs;

	// We cannot update a cbuffer until the GPU is done processing the commands
	// that reference it.  So each frame needs their own cbuffers.
	std::unique_ptr<Utils::UploadBuffer<PassConstants>> pass_cb = nullptr;
	std::unique_ptr<Utils::UploadBuffer<ObjectConstants>> object_cb = nullptr;
	std::unique_ptr<Utils::UploadBuffer<Vertex>> dynamic_geom_vb = nullptr;

	// Shadow maps are generated every frame
	std::unordered_map<std::string, ShadowMap> shadow_maps;

	// Fence value to mark commands up to this fence point.  This lets us
	// check if these frame resources are still in use by the GPU.
	UINT64 fence = 0;
};