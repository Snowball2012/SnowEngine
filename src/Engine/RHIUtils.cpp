#include "StdAfx.h"

#include "RHIUtils.h"

RHIBufferPtr RHIUtils::CreateInitializedGPUBuffer( RHI::BufferInfo& buffer_info, const void* src_data, size_t src_size )
{
    buffer_info.usage |= RHIBufferUsageFlags::TransferDst;

    RHI::BufferInfo upload_info = buffer_info;
    upload_info.usage = RHIBufferUsageFlags::TransferSrc;

    RHIUploadBufferPtr upload_buffer = GetRHI().CreateUploadBuffer( upload_info );
    upload_buffer->WriteBytes( src_data, src_size );

    RHIBufferPtr gpu_buffer = GetRHI().CreateDeviceBuffer( buffer_info );
    
    RHICommandList* list = BeginSingleTimeCommands( RHI::QueueType::Graphics );

    RHICommandList::CopyRegion copy_region = {};
    copy_region.src_offset = 0;
    copy_region.dst_offset = 0;
    copy_region.size = std::min( src_size, buffer_info.size );
    list->CopyBuffer( *upload_buffer->GetBuffer(), *gpu_buffer, 1, &copy_region );

    EndSingleTimeCommands( *list );

    return gpu_buffer;
}

RHIAccelerationStructurePtr RHIUtils::CreateBLAS( const RHIASGeometryInfo& geom )
{
    RHIASBuildSizes build_sizes = {};
    if ( !GetRHI().GetASBuildSize( RHIAccelerationStructureType::BLAS, &geom, 1, build_sizes ) )
        return nullptr;

    RHI::ASInfo as_create_info = {};
    as_create_info.size = build_sizes.as_size;
    as_create_info.type = RHIAccelerationStructureType::BLAS;

    RHIAccelerationStructurePtr as = GetRHI().CreateAS( as_create_info );
    if ( !as )
        return nullptr;

    RHI::BufferInfo scratch_ci = {};
    scratch_ci.size = build_sizes.scratch_size;
    scratch_ci.usage = RHIBufferUsageFlags::AccelerationStructureScratch;
    RHIBufferPtr scratch = GetRHI().CreateDeviceBuffer( scratch_ci );
    if ( !scratch )
        return nullptr;

    RHIASBuildInfo as_build_info = {};
    as_build_info.dst = as.get();
    as_build_info.scratch = scratch.get();
    as_build_info.geoms_count = 1;
    const RHIASGeometryInfo* geom_info = &geom;
    as_build_info.geoms = &geom_info;

    RHICommandList* list = BeginSingleTimeCommands( RHI::QueueType::Graphics );
    if ( !list )
        return nullptr;

    list->BuildAS( as_build_info );

    EndSingleTimeCommands( *list );

    return as;
}


RHICommandList* RHIUtils::BeginSingleTimeCommands( RHI::QueueType queue_type )
{
    RHICommandList* list = GetRHI().GetCommandList(queue_type);

    list->Begin();

    return list;
}

void RHIUtils::EndSingleTimeCommands( RHICommandList& list )
{
    list.End();

    RHI::SubmitInfo submit_info = {};
    submit_info.wait_semaphore_count = 0;
    submit_info.cmd_list_count = 1;
    RHICommandList* lists[] = { &list };
    submit_info.cmd_lists = lists;

    RHIFence completion_fence = GetRHI().SubmitCommandLists( submit_info );

    GetRHI().WaitForFenceCompletion( completion_fence );
}

uint8_t RHIUtils::GetRHIFormatSize( RHIFormat format )
{
    switch ( format )
    {
    case RHIFormat::Undefined:
        SE_ENSURE( false );
        return 0;
        break;
    case RHIFormat::R32G32_SFLOAT:
        return 8;
        break;
    case RHIFormat::R32G32B32_SFLOAT:
        return 12;
        break;
    case RHIFormat::B8G8R8A8_SRGB:
        return 4;
        break;
    case RHIFormat::R8G8B8A8_SRGB:
        return 4;
        break;
    case RHIFormat::R8G8B8A8_UNORM:
        return 4;
        break;
    default:
        NOTIMPL;
        break;
    }
    return 0;
}
