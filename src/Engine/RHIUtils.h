#pragma once

#include "StdAfx.h"

struct RHIUtils
{
    // Creates upload buffer under the hood, and transfers the data. Flushes rhi and waits for completion, so use with care. buffer_info must be filled
    // No const ref because the function appends TransferDst usage flag to buffer_info
    static RHIBufferPtr CreateInitializedGPUBuffer( RHI::BufferInfo& buffer_info, const void* src_data, size_t src_size );

    // Allows "immediate mode" controls. EndSingleTimeCommands flushes entire RHI, so use with caution
    static RHICommandList* BeginSingleTimeCommands( RHI::QueueType queue_type );
    static void EndSingleTimeCommands( RHICommandList& list );
};