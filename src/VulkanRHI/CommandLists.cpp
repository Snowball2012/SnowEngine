#include "StdAfx.h"

#include "CommandLists.h"

#include "VulkanRHIImpl.h"
#include "Buffers.h"
#include "PSO.h"
#include "AccelerationStructures.h"

VulkanCommandList::VulkanCommandList(VulkanRHI* rhi, RHI::QueueType type, CmdListId list_id)
    : m_rhi(rhi), m_type(type), m_list_id(list_id)
{

    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    switch (type)
    {
    case RHI::QueueType::Graphics:
        pool_info.queueFamilyIndex = m_rhi->GetQueueFamilyIndices().graphics.value();
        break;
    default:
        NOTIMPL;
        break;
    }

    VkDevice vk_device = m_rhi->GetDevice();
    VK_VERIFY(vkCreateCommandPool(vk_device, &pool_info, nullptr, &m_vk_cmd_pool));

    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = m_vk_cmd_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    VK_VERIFY(vkAllocateCommandBuffers(vk_device, &alloc_info, &m_vk_cmd_buffer));
}

VulkanCommandList::~VulkanCommandList()
{
    vkFreeCommandBuffers(m_rhi->GetDevice(), m_vk_cmd_pool, 1, &m_vk_cmd_buffer);

    vkDestroyCommandPool(m_rhi->GetDevice(), m_vk_cmd_pool, nullptr);
}

RHI::QueueType VulkanCommandList::GetType() const
{
    return m_type;
}

void VulkanCommandList::Begin()
{
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VK_VERIFY(vkBeginCommandBuffer(m_vk_cmd_buffer, &begin_info));
}

void VulkanCommandList::End()
{
    VK_VERIFY(vkEndCommandBuffer(m_vk_cmd_buffer));
    m_currently_bound_pso = nullptr;
}

void VulkanCommandList::CopyBuffer(RHIBuffer& src, RHIBuffer& dst, size_t region_count, CopyRegion* regions)
{
    VERIFY_NOT_EQUAL(region_count, 0);
    VERIFY_NOT_EQUAL(regions, nullptr);
    boost::container::small_vector<VkBufferCopy, 4> copy_regions;
    for (size_t i = 0; i < region_count; ++i)
    {
        auto& vk_region = copy_regions.emplace_back();
        vk_region.srcOffset = regions[i].src_offset;
        vk_region.dstOffset = regions[i].dst_offset;
        vk_region.size = regions[i].size;
    }

    vkCmdCopyBuffer(
        m_vk_cmd_buffer, RHIImpl(src).GetVkBuffer(), RHIImpl(dst).GetVkBuffer(),
        uint32_t(copy_regions.size()), copy_regions.data());
}

void VulkanCommandList::DrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
    PushConstantsIfNeeded();

    vkCmdDrawIndexed(m_vk_cmd_buffer, index_count, instance_count, first_index, vertex_offset, first_instance);
}

void VulkanCommandList::SetPSO(RHIGraphicsPipeline& pso)
{
    vkCmdBindPipeline(m_vk_cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RHIImpl(pso).GetVkPipeline());
    m_currently_bound_pso = &RHIImpl(pso);
}

void VulkanCommandList::SetVertexBuffers(uint32_t first_binding, const RHIBuffer* buffers, size_t buffers_count, const size_t* opt_offsets)
{
    boost::container::small_vector<VkBuffer, 4> vk_buffers;
    vk_buffers.resize(buffers_count);

    for (size_t i = 0; i < buffers_count; ++i)
    {
        VkBuffer& vk_buffer = vk_buffers[i];
        const VulkanBuffer& rhi_buffer = RHIImpl(buffers[i]);

        vk_buffer = rhi_buffer.GetVkBuffer();
    }

    const VkDeviceSize* offsets = opt_offsets;

    boost::container::small_vector<VkDeviceSize, 4> offsets_storage;
    if (!offsets)
    {
        offsets_storage.resize(buffers_count, 0);
        offsets = offsets_storage.data();
    }

    vkCmdBindVertexBuffers(
        m_vk_cmd_buffer, first_binding,
        uint32_t(buffers_count), vk_buffers.data(),
        offsets);
}

void VulkanCommandList::SetIndexBuffer(const RHIBuffer& index_buf, RHIIndexBufferType type, size_t offset)
{
    VkIndexType vk_type = VK_INDEX_TYPE_MAX_ENUM;
    switch (type)
    {
    case RHIIndexBufferType::UInt16: vk_type = VK_INDEX_TYPE_UINT16; break;
    case RHIIndexBufferType::UInt32: vk_type = VK_INDEX_TYPE_UINT32; break;
    default:
        NOTIMPL;
    }

    vkCmdBindIndexBuffer(m_vk_cmd_buffer, RHIImpl(index_buf).GetVkBuffer(), offset, vk_type);
}

void VulkanCommandList::BindDescriptorSet(size_t slot_idx, RHIDescriptorSet& set)
{
    VERIFY_NOT_EQUAL(m_currently_bound_pso, nullptr);

    VulkanDescriptorSet& set_impl = RHIImpl(set);

    VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
    VERIFY_EQUALS(m_type, RHI::QueueType::Graphics);

    set_impl.FlushBinds();
    VkDescriptorSet sets[] = { set_impl.GetVkDescriptorSet() };

    vkCmdBindDescriptorSets(
        m_vk_cmd_buffer, bind_point,
        m_currently_bound_pso->GetVkPipelineLayout(), uint32_t(slot_idx),
        uint32_t(std::size(sets)), sets, 0, nullptr);    
}

void VulkanCommandList::BeginPass(const RHIPassInfo& pass_info)
{
    boost::container::small_vector<VkRenderingAttachmentInfo, 4> vk_attachments;
    vk_attachments.resize(pass_info.render_targets_count);

    for (size_t i = 0; i < pass_info.render_targets_count; ++i)
    {
        auto& vk_attachment = vk_attachments[i];
        const auto& rhi_rt = pass_info.render_targets[i];

        vk_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

        memcpy(&vk_attachment.clearValue.color, &rhi_rt.clear_value, sizeof(vk_attachment.clearValue.color));

        vk_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        vk_attachment.imageView = RHIImpl(rhi_rt.rtv)->GetVkImageView();
        vk_attachment.loadOp = VulkanRHI::GetVkLoadOp(rhi_rt.load_op);
        vk_attachment.storeOp = VulkanRHI::GetVkStoreOp(rhi_rt.store_op);
    }

    VkRenderingInfo render_info = {};
    render_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    render_info.colorAttachmentCount = uint32_t(pass_info.render_targets_count);
    render_info.pColorAttachments = vk_attachments.data();
    render_info.renderArea.offset = VkOffset2D(pass_info.render_area.offset.x, pass_info.render_area.offset.y);
    render_info.renderArea.extent = VkExtent2D(pass_info.render_area.extent.x, pass_info.render_area.extent.y);
    render_info.layerCount = 1;

    vkCmdBeginRendering(m_vk_cmd_buffer, &render_info);
}

void VulkanCommandList::EndPass()
{
    vkCmdEndRendering(m_vk_cmd_buffer);
}

void VulkanCommandList::TextureBarriers(const RHITextureBarrier* barriers, size_t barrier_count)
{
    VkPipelineStageFlags src_stage = 0;
    VkPipelineStageFlags dst_stage = 0;

    boost::container::small_vector<VkImageMemoryBarrier, 4> vk_barriers;
    vk_barriers.resize(barrier_count);
    for (size_t i = 0; i < barrier_count; ++i)
    {
        const RHITextureBarrier& rhi_barrier = barriers[i];
        VkImageMemoryBarrier& vk_barrier = vk_barriers[i];
        VkImageLayout src_layout = VulkanRHI::GetVkImageLayout(rhi_barrier.layout_src);
        VkImageLayout dst_layout = VulkanRHI::GetVkImageLayout(rhi_barrier.layout_dst);

        VulkanRHI::GetStagesAndAccessMasksForLayoutBarrier(
            src_layout, dst_layout, src_stage, dst_stage, vk_barrier);

        vk_barrier.image = RHIImpl(rhi_barrier.texture)->GetVkImage();
        vk_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vk_barrier.subresourceRange.baseMipLevel = rhi_barrier.subresources.mip_base;
        vk_barrier.subresourceRange.levelCount = rhi_barrier.subresources.mip_count;
        vk_barrier.subresourceRange.baseArrayLayer = rhi_barrier.subresources.array_base;
        vk_barrier.subresourceRange.layerCount = rhi_barrier.subresources.array_count;
    }

    vkCmdPipelineBarrier(
        m_vk_cmd_buffer,
        src_stage, dst_stage, 0,
        0, nullptr,
        0, nullptr,
        uint32_t(vk_barriers.size()), vk_barriers.data());
}

void VulkanCommandList::CopyBufferToTexture(const RHIBuffer& buf, RHITexture& texture, const RHIBufferTextureCopyRegion* regions, size_t region_count)
{
    boost::container::small_vector<VkBufferImageCopy, 4> vk_regions;
    vk_regions.resize(region_count);
    for (size_t i = 0; i < region_count; ++i)
    {
        VkBufferImageCopy& vk_region = vk_regions[i];
        const RHIBufferTextureCopyRegion& rhi_region = regions[i];

        vk_region.bufferOffset = rhi_region.buffer_offset;
        vk_region.bufferRowLength = uint32_t(rhi_region.buffer_row_stride);
        vk_region.bufferImageHeight = uint32_t(rhi_region.buffer_texture_height);

        vk_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vk_region.imageSubresource.mipLevel = rhi_region.texture_subresource.mip_base;
        vk_region.imageSubresource.baseArrayLayer = rhi_region.texture_subresource.array_base;
        vk_region.imageSubresource.layerCount = rhi_region.texture_subresource.array_count;

        if (rhi_region.texture_subresource.mip_count > 1)
            NOTIMPL;

        vk_region.imageOffset = {
            int32_t(rhi_region.texture_offset[0]),
            int32_t(rhi_region.texture_offset[1]),
            int32_t(rhi_region.texture_offset[2])};

        vk_region.imageExtent = {
            uint32_t(rhi_region.texture_extent[0]),
            uint32_t(rhi_region.texture_extent[1]),
            uint32_t(rhi_region.texture_extent[2]) };
    }

    vkCmdCopyBufferToImage(
        m_vk_cmd_buffer,
        RHIImpl(buf).GetVkBuffer(), RHIImpl(texture).GetVkImage(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        uint32_t(vk_regions.size()), vk_regions.data());
}

void VulkanCommandList::SetViewports(size_t first_viewport, const RHIViewport* viewports, size_t viewports_count)
{
    boost::container::small_vector<VkViewport, 4> vk_viewports;
    vk_viewports.resize(viewports_count);

    for (size_t i = 0; i < viewports_count; ++i)
    {
        VkViewport& vk_viewport = vk_viewports[i];
        const RHIViewport& rhi_viewport = viewports[i];

        vk_viewport.x = rhi_viewport.x;
        vk_viewport.y = rhi_viewport.y;
        vk_viewport.width = rhi_viewport.width;
        vk_viewport.height = rhi_viewport.height;
        vk_viewport.minDepth = rhi_viewport.min_depth;
        vk_viewport.maxDepth = rhi_viewport.max_depth;
    }

    vkCmdSetViewport(m_vk_cmd_buffer, uint32_t(first_viewport), uint32_t(vk_viewports.size()), vk_viewports.data());
}

void VulkanCommandList::SetScissors(size_t first_scissor, const RHIRect2D* scissors, size_t scissors_count)
{
    boost::container::small_vector<VkRect2D, 4> vk_scissors;
    vk_scissors.resize(scissors_count);

    for (size_t i = 0; i < scissors_count; ++i)
    {
        VkRect2D& vk_scissor = vk_scissors[i];
        const RHIRect2D& rhi_scissor = scissors[i];

        vk_scissor.offset = VkOffset2D(rhi_scissor.offset.x, rhi_scissor.offset.y);
        vk_scissor.extent = VkExtent2D(rhi_scissor.extent.x, rhi_scissor.extent.y);
    }

    vkCmdSetScissor(m_vk_cmd_buffer, uint32_t(first_scissor), uint32_t(vk_scissors.size()), vk_scissors.data());
}

void VulkanCommandList::PushConstants( size_t offset, const void* data, size_t size )
{
    m_need_push_constants = true;

    if ( m_push_constants.size() < offset + size )
    {
        m_push_constants.resize( offset + size, 0 );
    }

    memcpy( m_push_constants.data() + offset, data, size );
}

void VulkanCommandList::BuildAS( const RHIASBuildInfo& info )
{
    constexpr size_t sv_size = 4;
    VkAccelerationStructureBuildGeometryInfoKHR vk_geom_build_info;
    bc::small_vector<VkAccelerationStructureBuildRangeInfoKHR, sv_size> vk_build_range_info;
    bc::small_vector<VkAccelerationStructureGeometryKHR, sv_size> vk_geometries;
    const uint32_t geoms_count = uint32_t( info.geoms_count );
    vk_build_range_info.resize( geoms_count );
    vk_geometries.resize( geoms_count );

    for ( uint32_t i = 0; i < geoms_count; ++i )
    {
        vk_build_range_info[i].firstVertex = 0;
        vk_build_range_info[i].primitiveOffset = 0;
        vk_build_range_info[i].transformOffset = 0;
        vk_build_range_info[i].primitiveCount = 0/*calc primitve count form geo*/;
    }

    vk_geom_build_info = {};
    vk_geom_build_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    vk_geom_build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    vk_geom_build_info.dstAccelerationStructure = RHIImpl( info.dst )->GetVkAS();
    vk_geom_build_info.scratchData.deviceAddress = RHIImpl( info.scratch )->GetDeviceAddress();
    vk_geom_build_info.geometryCount = geoms_count;
    vk_geom_build_info.pGeometries = vk_geometries.data();
    vk_geom_build_info.type = RHIImpl( info.dst )->GetVkType();

    const VkAccelerationStructureBuildRangeInfoKHR* range_infos = vk_build_range_info.data();
    vkCmdBuildAccelerationStructuresKHR( m_vk_cmd_buffer, 1, &vk_geom_build_info, &range_infos );
}

void VulkanCommandList::Reset()
{
    VK_VERIFY(vkResetCommandBuffer(m_vk_cmd_buffer, 0));
}

void VulkanCommandList::PushConstantsIfNeeded()
{
    if ( !m_need_push_constants )
        return;

    if ( !SE_ENSURE( m_currently_bound_pso ) )
        return;

    // As of now we only support 1 push constants range, so bind everything in one go.
    size_t num_push_constants = m_currently_bound_pso->GetPushConstantsCount();

    // Sanity check
    SE_ENSURE( m_push_constants.size() < 16 * SizeKB );

    size_t size_push_constants = num_push_constants * 4;

    if ( m_push_constants.size() != size_push_constants )
    {
        m_push_constants.resize( size_push_constants, 0 );
    }

    vkCmdPushConstants(
        m_vk_cmd_buffer, m_currently_bound_pso->GetVkPipelineLayout(),
        VK_SHADER_STAGE_ALL,
        0, uint32_t( size_push_constants ), m_push_constants.data() );

    m_need_push_constants = false;
}

VulkanCommandListManager::VulkanCommandListManager(VulkanRHI* rhi)
    : m_rhi(rhi)
{
    for (auto& id : m_deferred_layout_transitions_lists)
    {
        id = nullptr;
    }
}

VulkanCommandListManager::~VulkanCommandListManager()
{
    WaitSubmittedUntilCompletion();

    for (const auto& queue_submitted_lists : m_submitted_lists)
    {
        assert(queue_submitted_lists.size() == 0);
    }

    for (auto& queue_cmd_lists : m_cmd_lists)
    {
        queue_cmd_lists.clear();
    }

    for (VkFence fence : m_free_fences)
    {
        vkDestroyFence(m_rhi->GetDevice(), fence, nullptr);
    }
}

VulkanCommandList* VulkanCommandListManager::GetCommandList(RHI::QueueType type)
{
    VERIFY_EQUALS(type < RHI::QueueType::Count, true);

    auto& free_lists = m_free_lists[size_t(type)];
    if (!free_lists.empty())
    {
        auto list_id = free_lists.back();
        free_lists.pop_back();
        return m_cmd_lists[size_t(type)][list_id].get();
    }

    CmdListId list_id = m_cmd_lists[size_t(type)].emplace();

    VulkanCommandList* new_list = new VulkanCommandList(m_rhi, type, list_id);
    m_cmd_lists[size_t(type)][list_id].reset(new_list);

    return new_list;
}

RHIFence VulkanCommandListManager::SubmitCommandLists(const RHI::SubmitInfo& info)
{
    if (info.cmd_list_count == 0)
        return RHIFence{};

    const size_t queue_idx = size_t(info.cmd_lists[0]->GetType());

    // Not necessary, but we have to do this somewhere at regular intervals. Why not here?
    ProcessCompleted();

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    constexpr size_t typical_num_lists = 4;

    boost::container::small_vector<VkCommandBuffer, typical_num_lists> cmd_lists;
    boost::container::small_vector<VkSemaphore, typical_num_lists> semaphores_to_wait;
    boost::container::small_vector<VkPipelineStageFlags, typical_num_lists> stages_to_wait;

    const bool has_deferred_layout_transitions = m_deferred_layout_transitions_lists[queue_idx] != nullptr;

    const size_t total_cmd_list_count = info.cmd_list_count + (has_deferred_layout_transitions ? 1 : 0);
    cmd_lists.reserve(total_cmd_list_count);

    // Deferred transitions must be queued first
    if (has_deferred_layout_transitions)
    {
        cmd_lists.emplace_back(m_deferred_layout_transitions_lists[queue_idx]->GetVkCmdList());
    }
    for (size_t i = 0; i < info.cmd_list_count; ++i)
    {
        cmd_lists.emplace_back(RHIImpl(info.cmd_lists[i])->GetVkCmdList());
    }

    semaphores_to_wait.reserve(info.wait_semaphore_count);
    stages_to_wait.reserve(info.wait_semaphore_count);
    for (size_t i = 0; i < info.wait_semaphore_count; ++i)
    {
        semaphores_to_wait.emplace_back(static_cast<VulkanSemaphore*>(info.semaphores_to_wait[i])->GetVkSemaphore());
        stages_to_wait.emplace_back(VulkanRHI::GetVkPipelineStageFlags(info.stages_to_wait[i]));
    }

    submit_info.waitSemaphoreCount = uint32_t(semaphores_to_wait.size());
    submit_info.pWaitSemaphores = semaphores_to_wait.data();
    submit_info.pWaitDstStageMask = stages_to_wait.data();

    submit_info.commandBufferCount = uint32_t(cmd_lists.size());
    submit_info.pCommandBuffers = cmd_lists.data();

    VkSemaphore semaphore_to_signal = info.semaphore_to_signal ? static_cast<VulkanSemaphore*>(info.semaphore_to_signal)->GetVkSemaphore() : VK_NULL_HANDLE;
    submit_info.signalSemaphoreCount = info.semaphore_to_signal ? 1 : 0;
    submit_info.pSignalSemaphores = (semaphore_to_signal != VK_NULL_HANDLE) ? &semaphore_to_signal : nullptr;

    auto& submitted_lists = m_submitted_lists[queue_idx].emplace();
    submitted_lists.completion_fence = VK_NULL_HANDLE;
    
    if (!m_free_fences.empty())
    {
        submitted_lists.completion_fence = m_free_fences.back();
        m_free_fences.pop_back();
    }
    else
    {
        VkFenceCreateInfo fence_create_info = {};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VK_VERIFY(vkCreateFence(m_rhi->GetDevice(), &fence_create_info, nullptr, &submitted_lists.completion_fence));
    }

    submitted_lists.lists.reserve(total_cmd_list_count);
    if (has_deferred_layout_transitions)
    {
        m_deferred_layout_transitions_lists[queue_idx]->End();
        submitted_lists.lists.emplace_back(m_deferred_layout_transitions_lists[queue_idx]);
        m_deferred_layout_transitions_lists[queue_idx] = nullptr;
    }
    for (size_t i = 0; i < info.cmd_list_count; ++i)
        submitted_lists.lists.emplace_back(RHIImpl(*(info.cmd_lists + i)));    

    VulkanQueue* vk_queue = m_rhi->GetQueue(info.cmd_lists[0]->GetType());

    VERIFY_NOT_EQUAL(vk_queue, nullptr);

    VK_VERIFY(vkQueueSubmit(vk_queue->vk_handle, 1, &submit_info, submitted_lists.completion_fence));
    vk_queue->submitted_counter++;

    return RHIFence{
        reinterpret_cast<uint64_t>(submitted_lists.completion_fence),
        vk_queue->submitted_counter,
        static_cast<uint64_t>(info.cmd_lists[0]->GetType()),
    };
}

void VulkanCommandListManager::ProcessCompleted()
{
    boost::container::small_vector<VkFence, 8> fences_to_reset;
    for (size_t queue_i = 0; queue_i < m_submitted_lists.size(); ++queue_i)
    {
        auto& submitted_lists = m_submitted_lists[queue_i];
        auto& free_lists = m_free_lists[queue_i];
        VulkanQueue* vk_queue = m_rhi->GetQueue(RHI::QueueType(queue_i));
        VERIFY_NOT_EQUAL(vk_queue, nullptr);

        while(!submitted_lists.empty())
        {
            auto& submitted_info = submitted_lists.front();
            VkResult fence_status = vkGetFenceStatus(m_rhi->GetDevice(), submitted_info.completion_fence);
            VERIFY_EQUALS(fence_status == VK_SUCCESS || fence_status == VK_NOT_READY, true);
            const bool fence_completed = fence_status == VK_SUCCESS;
            if (!fence_completed)
                break;

            vk_queue->completed_counter++;

            for (VulkanCommandList* list : submitted_info.lists)
                free_lists.emplace_back(list->GetListId());

            fences_to_reset.push_back(submitted_info.completion_fence);

            m_free_fences.push_back(submitted_info.completion_fence);

            submitted_lists.pop();
        }
    }

    if (!fences_to_reset.empty())
        vkResetFences(m_rhi->GetDevice(), uint32_t(fences_to_reset.size()), fences_to_reset.data());
}

void VulkanCommandListManager::WaitSubmittedUntilCompletion()
{
    boost::container::static_vector<VkFence, size_t(RHI::QueueType::Count)> fences_to_wait;
    for (size_t queue_i = 0; queue_i < m_submitted_lists.size(); ++queue_i)
    {
        auto& submitted_lists = m_submitted_lists[queue_i];

        if (!submitted_lists.empty())
            fences_to_wait.push_back(submitted_lists.back().completion_fence);
            
    }
    VK_VERIFY(vkWaitForFences(m_rhi->GetDevice(), uint32_t(fences_to_wait.size()), fences_to_wait.data(), VK_TRUE, std::numeric_limits<uint64_t>::max()));

    ProcessCompleted();
}

void VulkanCommandListManager::DeferImageLayoutTransition(VkImage image, RHI::QueueType queue_type, VkImageLayout old_layout, VkImageLayout new_layout)
{
    VERIFY_EQUALS(queue_type < RHI::QueueType::Count, true);
    VulkanCommandList*& cmd_list = m_deferred_layout_transitions_lists[size_t(queue_type)];

    if (!cmd_list)
    {
        cmd_list = RHIImpl(m_rhi->GetCommandList(queue_type));
        cmd_list->Begin();
    }

    m_rhi->TransitionImageLayout(cmd_list->GetVkCmdList(), image, old_layout, new_layout);
}
