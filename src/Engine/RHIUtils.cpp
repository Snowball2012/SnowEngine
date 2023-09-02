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

RHITexturePtr RHIUtils::CreateInitializedGPUTexture( RHI::TextureInfo& texture_info, const void* src_data, size_t src_size )
{
    RHITextureLayout desired_initial_layout = texture_info.initial_layout;
    texture_info.initial_layout = RHITextureLayout::TransferDst;
    texture_info.usage |= RHITextureUsageFlags::TransferDst;

    RHI::BufferInfo upload_info = {};
    upload_info.size = src_size;
    upload_info.usage = RHIBufferUsageFlags::TransferSrc;

    RHIUploadBufferPtr upload_buffer = GetRHI().CreateUploadBuffer( upload_info );
    upload_buffer->WriteBytes( src_data, src_size );

    RHITexturePtr gpu_texture = GetRHI().CreateTexture( texture_info );

    RHICommandList* list = BeginSingleTimeCommands( RHI::QueueType::Graphics );

    RHIBufferTextureCopyRegion copy_region = {};
    copy_region.texture_subresource.mip_count = texture_info.mips;
    copy_region.texture_subresource.array_count = texture_info.array_layers;
    copy_region.texture_extent[0] = texture_info.width;
    copy_region.texture_extent[1] = texture_info.height;
    copy_region.texture_extent[2] = texture_info.depth;
    list->CopyBufferToTexture( *upload_buffer->GetBuffer(), *gpu_texture, &copy_region, 1 );

    if ( texture_info.initial_layout != desired_initial_layout )
    {
        RHITextureBarrier barrier;
        barrier.layout_src = texture_info.initial_layout;
        barrier.layout_dst = desired_initial_layout;
        barrier.texture = gpu_texture.get();
        list->TextureBarriers( &barrier, 1 );
    }

    EndSingleTimeCommands( *list );

    texture_info.initial_layout = desired_initial_layout;

    return gpu_texture;
}

RHIAccelerationStructurePtr RHIUtils::CreateAS( const RHIASGeometryInfo& geom )
{
    RHIAccelerationStructureType type = RHIAccelerationStructureType::BLAS;
    
    switch ( geom.type )
    {
    case RHIASGeometryType::Instances:
        type = RHIAccelerationStructureType::TLAS;
        break;
    case RHIASGeometryType::Triangles:
        type = RHIAccelerationStructureType::BLAS;
        break;
    default:
        NOTIMPL;
        return nullptr;
    }

    RHIASBuildSizes build_sizes = {};
    if ( !GetRHI().GetASBuildSize( type, &geom, 1, build_sizes ) )
        return nullptr;

    RHI::ASInfo as_create_info = {};
    as_create_info.size = build_sizes.as_size;
    as_create_info.type = type;

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

bool TLAS::Build( RHICommandList& cmd_list )
{
    RHIASInstanceBufferPtr& gpu_instance_buf = m_gpu_instances[m_cur_buf_idx++];
    m_cur_buf_idx = m_cur_buf_idx % m_max_num_bufs;

    RHIASBuildSizes build_sizes = {};

    RHIASGeometryInfo geom_info = {};
    geom_info.type = RHIASGeometryType::Instances;

    if ( m_instances.empty() )
        NOTIMPL;

    if ( !gpu_instance_buf )
    {
        RHI::ASInstanceBufferInfo instance_buffer_ci = {};
        instance_buffer_ci.data = m_instances.data();
        instance_buffer_ci.num_instances = m_instances.size();
        gpu_instance_buf = GetRHI().CreateASInstanceBuffer( instance_buffer_ci );
    }
    
    if ( !SE_ENSURE( m_gpu_instances != nullptr ) )
        return false;

    gpu_instance_buf->UpdateBuffer( m_instances.data(), m_instances.size() );

    geom_info.instances.instance_buf = gpu_instance_buf.get();
    geom_info.instances.num_instances = m_instances.size();

    GetRHI().GetASBuildSize( RHIAccelerationStructureType::TLAS, &geom_info, 1, build_sizes );

    bool need_full_build = 
        !m_as || !m_scratch
        || ( m_scratch->GetSize() < build_sizes.scratch_size )
        || ( m_as->GetSize() < build_sizes.as_size );

    if ( need_full_build )
    {
        RHI::ASInfo as_create_info = {};
        as_create_info.size = build_sizes.as_size;
        as_create_info.size += as_create_info.size / 4; // make some room for subsequent rebuilds
        as_create_info.type = RHIAccelerationStructureType::TLAS;

        m_as = GetRHI().CreateAS( as_create_info );
        if ( !m_as )
            return false;

        RHI::BufferInfo scratch_ci = {};
        scratch_ci.size = build_sizes.scratch_size;
        scratch_ci.size += scratch_ci.size / 4; // make some room for subsequent rebuilds
        scratch_ci.usage = RHIBufferUsageFlags::AccelerationStructureScratch;
        m_scratch = GetRHI().CreateDeviceBuffer( scratch_ci );
        if ( !m_scratch )
            return false;
    }

    if ( !SE_ENSURE( m_as != nullptr && m_scratch != nullptr && m_gpu_instances != nullptr ) )
        return false;

    const RHIASGeometryInfo* geom_info_ptr = &geom_info;
    RHIASBuildInfo build_info = {};
    build_info.geoms = &geom_info_ptr;
    build_info.geoms_count = 1;
    build_info.dst = m_as.get();
    build_info.scratch = m_scratch.get();

    cmd_list.BuildAS( build_info );

    return true;
}

void TLAS::Reset()
{
    m_as = nullptr;
    m_scratch = nullptr;
    for ( auto& instance_buf : m_gpu_instances )
        instance_buf = nullptr;
}
