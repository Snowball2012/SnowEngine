#pragma once

#include "RenderUtils.h"

#include "RenderData.h"
#include "SceneImporter.h"

#include "GPUTaskQueue.h"

struct FrameResource
{
	FrameResource( ID3D12Device* device, size_t cmd_allocator_count );

	// We cannot reset the allocator until the GPU is done processing the commands.
	// So each frame needs their own allocator.
	std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> cmd_list_allocs;

	// Fence value to mark commands up to this fence point.  This lets us
	// check if these frame resources are still in use by the GPU.
	GPUTaskQueue::Timestamp available_timestamp = 0;
};