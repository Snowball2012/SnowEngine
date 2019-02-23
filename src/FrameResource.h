#pragma once

#include "GPUTaskQueue.h"

struct FrameResource
{
	// Fence value to mark commands up to this fence point.  This lets us
	// check if these frame resources are still in use by the GPU.
	GPUTaskQueue::Timestamp available_timestamp = 0;
};