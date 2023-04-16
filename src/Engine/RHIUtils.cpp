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