#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

class VulkanBuffer : public RHIBuffer
{
	class VulkanRHI* m_rhi = nullptr;
	VkBuffer m_vk_buffer = VK_NULL_HANDLE;
	VmaAllocation m_allocation = VK_NULL_HANDLE;

public:
	VulkanBuffer(VulkanRHI* rhi, const RHI::BufferInfo& info, VmaAllocationCreateFlags alloc_create_flags);

	virtual ~VulkanBuffer() override;

	// Inherited via RHIBuffer
	virtual void AddRef() override {}
	virtual void Release() override {}

	virtual void* GetNativeBuffer() const override { return (void*)&m_vk_buffer; }

	void GetAllocationInfo(VmaAllocationInfo& info);
};

class VulkanUploadBuffer : public RHIUploadBuffer
{
	RHIObjectPtr<VulkanBuffer> m_buffer = nullptr;

public:
	VulkanUploadBuffer(VulkanRHI* rhi, const RHI::BufferInfo& info);

	virtual ~VulkanUploadBuffer() override;

	// Inherited via RHIUploadBuffer
	virtual void AddRef() override;
	virtual void Release() override;

	virtual void WriteBytes(const void* src, size_t size, size_t offset) override;

	virtual RHIBuffer* GetBuffer() override { return m_buffer.get(); }
	virtual const RHIBuffer* GetBuffer() const override { return m_buffer.get(); }
};
