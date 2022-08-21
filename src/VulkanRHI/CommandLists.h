#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

class VulkanCommandListManager
{
private:
	class VulkanRHI* m_rhi = nullptr;

	std::array<packed_freelist<std::unique_ptr<class VulkanCommandList>>, size_t(RHI::QueueType::Count)> m_cmd_lists;
	std::vector<VkFence> m_free_fences;

public:
	using CmdListId = std::remove_reference_t<decltype(m_cmd_lists[0])>::id;

private:

	std::array<std::vector<CmdListId>, size_t(RHI::QueueType::Count)> m_free_lists;

	struct SubmittedListsInfo
	{
		std::vector<VulkanCommandList*> lists;
		VkFence completion_fence = VK_NULL_HANDLE;
	};
	std::array<std::queue<SubmittedListsInfo>, size_t(RHI::QueueType::Count)> m_submitted_lists;

public:
	VulkanCommandListManager(VulkanRHI* rhi);
	~VulkanCommandListManager();

	VulkanCommandList* GetCommandList(RHI::QueueType type);
	RHIFence SubmitCommandLists(const RHI::SubmitInfo& info);

	void ProcessCompleted();

	void WaitSubmittedUntilCompletion();

};

class VulkanCommandList : public RHICommandList
{
public:
	using CmdListId = VulkanCommandListManager::CmdListId;

private:

	VkCommandBuffer m_vk_cmd_buffer = VK_NULL_HANDLE;
	VkCommandPool m_vk_cmd_pool = VK_NULL_HANDLE;
	RHI::QueueType m_type = RHI::QueueType::Graphics;
	CmdListId m_list_id = CmdListId::nullid;
	class VulkanRHI* m_rhi = nullptr;

public:
	VulkanCommandList(VulkanRHI* rhi, RHI::QueueType type, CmdListId list_id);
	virtual ~VulkanCommandList();

	virtual void* GetNativeCmdList() const { return (void*)&m_vk_cmd_buffer; }

	virtual RHI::QueueType GetType() const override;

	virtual void Begin() override;
	virtual void End() override;

	virtual void CopyBuffer(RHIBuffer& src, RHIBuffer& dst, size_t region_count, CopyRegion* regions) override;

	CmdListId GetListId() const { return m_list_id; }

	VkCommandBuffer GetVkCmdList() const { return m_vk_cmd_buffer; }
};
