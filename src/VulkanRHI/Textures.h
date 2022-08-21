#pragma once

#include "StdAfx.h"

#include "RHI/RHI.h"

// This wrapper does not actually owns a texture. Swapchain have to return RHITexture wrappers for it's images, but this wrappers mustn't own the texture
class VulkanTextureBase : public RHITexture
{
protected:
	VkImage m_image = VK_NULL_HANDLE;

public:
	VulkanTextureBase() = default;
	VulkanTextureBase(VkImage image);

	virtual ~VulkanTextureBase() override {}

	// Inherited via RHITexture
	virtual void AddRef() override;
	virtual void Release() override;

	virtual void* GetNativeTexture() const { return (void*)&m_image; }

	VkImage GetVkImage() const { return m_image; }
};

class VulkanTexture : public VulkanTextureBase
{
	class VulkanRHI* m_rhi = nullptr;
	VmaAllocation m_allocation = VK_NULL_HANDLE;

public:
	VulkanTexture(VulkanRHI* rhi, const RHI::TextureInfo& info);

	virtual ~VulkanTexture() override;

	// Inherited via RHITexture
	virtual void AddRef() override;
	virtual void Release() override;
};