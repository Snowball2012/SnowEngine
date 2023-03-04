#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

class VulkanBuffer : public RHIBuffer
{
	GENERATE_RHI_OBJECT_BODY()

	VkBuffer m_vk_buffer = VK_NULL_HANDLE;
	VmaAllocation m_allocation = VK_NULL_HANDLE;

public:
	VulkanBuffer(VulkanRHI* rhi, const RHI::BufferInfo& info, VmaMemoryUsage usage, VmaAllocationCreateFlags alloc_create_flags, VkMemoryPropertyFlags alloc_required_flags );

	virtual ~VulkanBuffer() override;

	virtual size_t GetSize() const override;

	void GetAllocationInfo(VmaAllocationInfo& info) const;

	VkBuffer GetVkBuffer() const { return m_vk_buffer; }
};
IMPLEMENT_RHI_INTERFACE(RHIBuffer, VulkanBuffer)

class VulkanUploadBuffer : public RHIUploadBuffer
{
	GENERATE_RHI_OBJECT_BODY()

	RHIObjectPtr<VulkanBuffer> m_buffer = nullptr;

public:
	VulkanUploadBuffer(VulkanRHI* rhi, const RHI::BufferInfo& info);

	virtual ~VulkanUploadBuffer() override;

	virtual void WriteBytes(const void* src, size_t size, size_t offset) override;

	virtual RHIBuffer* GetBuffer() override { return m_buffer.get(); }
	virtual const RHIBuffer* GetBuffer() const override { return m_buffer.get(); }
};
IMPLEMENT_RHI_INTERFACE(RHIUploadBuffer, VulkanUploadBuffer)