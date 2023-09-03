#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

#include "Buffers.h"
#include "Textures.h"

class VulkanCBV : public RHIUniformBufferView
{
	GENERATE_RHI_OBJECT_BODY()

	RHIObjectPtr<VulkanBuffer> m_buffer = nullptr;

	VkDescriptorBufferInfo m_view_info = {};
public:
	virtual ~VulkanCBV() override;

	VulkanCBV(VulkanRHI* rhi, const RHIUniformBufferViewInfo& info);

	const VkDescriptorBufferInfo* GetVkBufferInfo() const { return &m_view_info; }
};
IMPLEMENT_RHI_INTERFACE(RHIUniformBufferView, VulkanCBV)

class VulkanTextureSRV : public RHITextureROView
{
	GENERATE_RHI_OBJECT_BODY()

	RHIObjectPtr<VulkanTextureBase> m_texture = nullptr;

	VkImageView m_vk_image_view = VK_NULL_HANDLE;

	VkDescriptorImageInfo m_view_info = {};
public:
	virtual ~VulkanTextureSRV() override;

	VulkanTextureSRV(VulkanRHI* rhi, const RHI::TextureROViewInfo& info);

	const VkDescriptorImageInfo* GetVkImageInfo() const { return &m_view_info; }
};
IMPLEMENT_RHI_INTERFACE(RHITextureROView, VulkanTextureSRV)

class VulkanTextureRWView : public RHITextureRWView
{
	GENERATE_RHI_OBJECT_BODY()

	RHIObjectPtr<VulkanTextureBase> m_texture = nullptr;

	VkImageView m_vk_image_view = VK_NULL_HANDLE;

	VkDescriptorImageInfo m_view_info = {};
public:
	virtual ~VulkanTextureRWView() override;

	VulkanTextureRWView( VulkanRHI* rhi, const RHI::TextureRWViewInfo& info );

	const VkDescriptorImageInfo* GetVkImageInfo() const { return &m_view_info; }
};
IMPLEMENT_RHI_INTERFACE( RHITextureRWView, VulkanTextureRWView )

class VulkanRTV : public RHIRenderTargetView
{
	GENERATE_RHI_OBJECT_BODY()

	RHIObjectPtr<VulkanTextureBase> m_texture = nullptr;

	VkImageView m_vk_image_view = VK_NULL_HANDLE;
public:
	virtual ~VulkanRTV() override;

	VulkanRTV(VulkanRHI* rhi, const RHI::RenderTargetViewInfo& info);

	virtual glm::uvec3 GetSize() const override;

	VkImageView GetVkImageView() const { return m_vk_image_view; }
};
IMPLEMENT_RHI_INTERFACE(RHIRenderTargetView, VulkanRTV)

