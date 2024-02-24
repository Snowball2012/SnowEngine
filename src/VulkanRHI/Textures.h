#pragma once

#include "StdAfx.h"

#include "RHI/RHI.h"

// This wrapper does not actually owns a texture. Swapchain have to return RHITexture wrappers for it's images, but this wrappers mustn't own the texture
// That's why it does not use GENERATE_RHIOBJECT_BODY macro
class VulkanTextureBase : public RHITexture
{
protected:
	VkImage m_image = VK_NULL_HANDLE;

	RHI::TextureInfo m_info = {};

	RHITextureRWViewPtr m_base_rw_view = nullptr;

public:
	VulkanTextureBase() = default;
	VulkanTextureBase(VkImage image, const RHI::TextureInfo& info);

	virtual ~VulkanTextureBase() override {}

	// Inherited via RHITexture
	// Intentionally blank
	virtual void AddRef() override {}
	virtual void Release() override {}

	RHITextureRWView* GetBaseRWView() const override { return m_base_rw_view.get(); }

	VkImage GetVkImage() const { return m_image; }

	const RHI::TextureInfo& GetInfo() const { return m_info; }
};
IMPLEMENT_RHI_INTERFACE(RHITexture, VulkanTextureBase)

class VulkanTexture : public VulkanTextureBase
{
	GENERATE_RHI_OBJECT_BODY()

	VmaAllocation m_allocation = VK_NULL_HANDLE;

public:
	VulkanTexture(VulkanRHI* rhi, const RHI::TextureInfo& info);

	virtual ~VulkanTexture() override;
};

class VulkanSampler : public RHISampler
{
	GENERATE_RHI_OBJECT_BODY()

	VkSampler m_sampler = VK_NULL_HANDLE;

	VkDescriptorImageInfo m_desc_info = {};
public:
	VulkanSampler(VulkanRHI* rhi, const RHI::SamplerInfo& info);

	virtual ~VulkanSampler() override;

	const VkDescriptorImageInfo* GetVkImageInfo() const { return &m_desc_info; }
};
IMPLEMENT_RHI_INTERFACE(RHISampler, VulkanSampler)