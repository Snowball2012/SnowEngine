#include "StdAfx.h"

#include "CommandLists.h"

#include "VulkanRHIImpl.h"

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

VulkanCommandListManager::VulkanCommandListManager(VulkanRHI* rhi)
    : m_rhi(rhi)
{
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

    // Not necessary, but we have to do this somewhere at regular intervals. Why not here?
    ProcessCompleted();

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    constexpr size_t typical_num_lists = 4;

    boost::container::small_vector<VkCommandBuffer, typical_num_lists> cmd_lists;
    boost::container::small_vector<VkSemaphore, typical_num_lists> semaphores_to_wait;
    boost::container::small_vector<VkPipelineStageFlags, typical_num_lists> stages_to_wait;

    cmd_lists.reserve(info.cmd_list_count);
    for (size_t i = 0; i < info.cmd_list_count; ++i)
    {
        cmd_lists.emplace_back(static_cast<VulkanCommandList*>(info.cmd_lists[i])->GetVkCmdList());
    }

    semaphores_to_wait.reserve(info.wait_semaphore_count);
    stages_to_wait.reserve(info.wait_semaphore_count);
    for (size_t i = 0; i < info.wait_semaphore_count; ++i)
    {
        semaphores_to_wait.emplace_back(static_cast<VulkanSemaphore*>(info.semaphores_to_wait[i])->GetVkSemaphore());
        stages_to_wait.emplace_back(VulkanRHI::GetVkStageFlags(info.stages_to_wait[i]));
    }

    submit_info.waitSemaphoreCount = uint32_t(semaphores_to_wait.size());
    submit_info.pWaitSemaphores = semaphores_to_wait.data();
    submit_info.pWaitDstStageMask = stages_to_wait.data();

    submit_info.commandBufferCount = uint32_t(cmd_lists.size());
    submit_info.pCommandBuffers = cmd_lists.data();

    VkSemaphore semaphore_to_signal = info.semaphore_to_signal ? static_cast<VulkanSemaphore*>(info.semaphore_to_signal)->GetVkSemaphore() : VK_NULL_HANDLE;
    submit_info.signalSemaphoreCount = info.semaphore_to_signal ? 1 : 0;
    submit_info.pSignalSemaphores = (semaphore_to_signal != VK_NULL_HANDLE) ? &semaphore_to_signal : nullptr;

    auto& submitted_lists = m_submitted_lists[size_t(info.cmd_lists[0]->GetType())].emplace();
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
    submitted_lists.lists.reserve(info.cmd_list_count);
    for (size_t i = 0; i < info.cmd_list_count; ++i)
        submitted_lists.lists.emplace_back(static_cast<VulkanCommandList*>(*(info.cmd_lists + i)));

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
