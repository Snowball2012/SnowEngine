#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

#include "Buffers.h"
#include "Textures.h"

class VulkanCBV : public RHICBV
{
	GENERATE_RHI_OBJECT_BODY()

	RHIObjectPtr<VulkanBuffer> m_buffer = nullptr;

	VkDescriptorBufferInfo m_view_info = {};
public:
	virtual ~VulkanCBV() override;

	VulkanCBV(VulkanRHI* rhi, const RHI::CBVInfo& info);

	const VkDescriptorBufferInfo* GetVkBufferInfo() const { return &m_view_info; }
};
IMPLEMENT_RHI_INTERFACE(RHICBV, VulkanCBV)

class VulkanTextureSRV : public RHITextureSRV
{
	GENERATE_RHI_OBJECT_BODY()

	RHIObjectPtr<VulkanTextureBase> m_texture = nullptr;

	VkImageView m_vk_image_view = VK_NULL_HANDLE;

	VkDescriptorImageInfo m_view_info = {};
public:
	virtual ~VulkanTextureSRV() override;

	VulkanTextureSRV(VulkanRHI* rhi, const RHI::TextureSRVInfo& info);

	const VkDescriptorImageInfo* GetVkImageInfo() const { return &m_view_info; }
};
IMPLEMENT_RHI_INTERFACE(RHITextureSRV, VulkanTextureSRV)

