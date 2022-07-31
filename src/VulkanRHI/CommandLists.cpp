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

VulkanCommandListManager::VulkanCommandListManager(VulkanRHI* rhi)
    : m_rhi(rhi)
{
}

VulkanCommandListManager::~VulkanCommandListManager()
{
    WaitSubmittedUntilCompletion();
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

void VulkanCommandListManager::SubmitCommandLists(const RHI::SubmitInfo& info)
{
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



    VK_VERIFY(vkQueueSubmit(m_graphics_queue, 1, &submit_info, m_inflight_fences[m_current_frame]));
}
