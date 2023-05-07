#pragma once

#include "StdAfx.h"

class TLAS
{
    RHIAccelerationStructurePtr m_as = nullptr;
    RHIBufferPtr m_scratch = nullptr;
    packed_freelist<RHIASInstanceData> m_instances;

    static const constexpr int m_max_num_bufs = 3;
    RHIASInstanceBufferPtr m_gpu_instances[m_max_num_bufs] = {};
    size_t m_cur_buf_idx = 0;

public:
    using InstanceID = decltype( m_instances )::id;

    bool Build( RHICommandList& cmd_list );

    void Reset(); // clears memory, releases rhi objects

    RHIAccelerationStructure& GetRHIAS() const { return *m_as; }

    auto& Instances() { return m_instances; };
    const auto& Instances() const { return m_instances; }
};

struct RHIUtils
{
    // Creates upload buffer under the hood, and transfers the data. Flushes rhi and waits for completion, so use with care. buffer_info must be filled
    // No const ref because the function appends TransferDst usage flag to buffer_info
    static RHIBufferPtr CreateInitializedGPUBuffer( RHI::BufferInfo& buffer_info, const void* src_data, size_t src_size );

    // Super slow because of the flushes and lots of allocations
    static RHIAccelerationStructurePtr CreateAS( const RHIASGeometryInfo& geom );

    // Allows "immediate mode" controls. EndSingleTimeCommands flushes entire RHI, so use with caution
    static RHICommandList* BeginSingleTimeCommands( RHI::QueueType queue_type );
    static void EndSingleTimeCommands( RHICommandList& list );

    static uint8_t GetRHIFormatSize( RHIFormat format );
};