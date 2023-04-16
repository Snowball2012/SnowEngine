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

	std::array<VulkanCommandList*, size_t(RHI::QueueType::Count)> m_deferred_layout_transitions_lists;

public:
	VulkanCommandListManager(VulkanRHI* rhi);
	~VulkanCommandListManager();

	VulkanCommandList* GetCommandList(RHI::QueueType type);
	RHIFence SubmitCommandLists(const RHI::SubmitInfo& info);

	void ProcessCompleted();

	void WaitSubmittedUntilCompletion();

	void DeferImageLayoutTransition(VkImage image, RHI::QueueType queue, VkImageLayout old_layout, VkImageLayout new_layout);

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

	class VulkanGraphicsPSO* m_currently_bound_pso = nullptr;

	std::vector<uint8_t> m_push_constants;
	bool m_need_push_constants = false;

public:
	VulkanCommandList(VulkanRHI* rhi, RHI::QueueType type, CmdListId list_id);
	virtual ~VulkanCommandList();

	virtual RHI::QueueType GetType() const override;

	virtual void Begin() override;
	virtual void End() override;

	virtual void CopyBuffer(RHIBuffer& src, RHIBuffer& dst, size_t region_count, CopyRegion* regions) override;

	virtual void DrawIndexed(
		uint32_t index_count,
		uint32_t instance_count,
		uint32_t first_index,
		int32_t vertex_offset,
		uint32_t first_instance) override;

	virtual void SetPSO(RHIGraphicsPipeline& pso) override;

	virtual void SetVertexBuffers(uint32_t first_binding, const RHIBuffer* buffers, size_t buffers_count, const size_t* opt_offsets) override;
	virtual void SetIndexBuffer( const RHIBuffer& index_buf, RHIIndexBufferType type, size_t offset ) override;

	virtual void BindDescriptorSet(size_t slot_idx, RHIDescriptorSet& table) override;

	virtual void BeginPass(const RHIPassInfo& pass_info) override;
	virtual void EndPass() override;

	virtual void TextureBarriers(const RHITextureBarrier* barriers, size_t barrier_count) override;

	virtual void CopyBufferToTexture(
		const RHIBuffer& buf, RHITexture& texture,
		const RHIBufferTextureCopyRegion* regions, size_t region_count) override;

	virtual void SetViewports(size_t first_viewport, const RHIViewport* viewports, size_t viewports_count) override;
	virtual void SetScissors(size_t first_scissor, const RHIRect2D* scissors, size_t scissors_count) override;

	virtual void PushConstants( size_t offset, const void* data, size_t size ) override;

	CmdListId GetListId() const { return m_list_id; }

	VkCommandBuffer GetVkCmdList() const { return m_vk_cmd_buffer; }

	void Reset();

private:
	void PushConstantsIfNeeded();
};

IMPLEMENT_RHI_INTERFACE(RHICommandList, VulkanCommandList)