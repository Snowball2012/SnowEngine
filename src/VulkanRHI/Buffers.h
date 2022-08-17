#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

class VulkanUploadBuffer : public RHIUploadBuffer
{
	class VulkanRHI* m_rhi = nullptr;
	VkBuffer m_vk_buffer = VK_NULL_HANDLE;
	VmaAllocation m_allocation = VK_NULL_HANDLE;

public:

	VulkanUploadBuffer(VulkanRHI* rhi, const RHI::UploadBufferInfo& info);
	virtual ~VulkanUploadBuffer() override;

	// Inherited via RHIUploadBuffer
	virtual void AddRef() override;
	virtual void Release() override;

	virtual void* GetNativeBuffer() const override { return (void*)&m_vk_buffer; }
};