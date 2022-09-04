#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

#include "Textures.h"
#include "ResourceViews.h"

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities = {};
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> present_modes;
};

struct VulkanSwapChainCreateInfo : SwapChainCreateInfo
{
	VkSurfaceKHR surface = VK_NULL_HANDLE;
};

class VulkanSwapChain : public RHISwapChain
{
	GENERATE_RHI_OBJECT_BODY()

private:
	VkSurfaceKHR m_surface = VK_NULL_HANDLE;
	VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
	std::vector<RHIObjectPtr<VulkanTextureBase>> m_swapchain_images;
	std::vector<RHIObjectPtr<VulkanRTV>> m_swapchain_image_views;
	VkExtent2D m_swapchain_size_pixels = { 0, 0 };
	VkSurfaceFormatKHR m_swapchain_format = {};

	const VulkanSwapChainCreateInfo m_create_info;

	uint32_t m_cur_image_index = -1;

public:

	VulkanSwapChain(class VulkanRHI* rhi, const VulkanSwapChainCreateInfo& create_info);

	virtual ~VulkanSwapChain() override;

	// Inherited via RHISwapChain
	virtual glm::uvec2 GetExtent() const override;
	virtual void AcquireNextImage(class RHISemaphore* semaphore_to_signal, bool& out_recreated) override;
	virtual void Recreate() override;
	virtual RHIFormat GetFormat() const override;
	virtual RHITexture* GetTexture() override { return m_swapchain_images[m_cur_image_index].get(); }

	virtual RHIRTV* GetRTV() override { return m_swapchain_image_views[m_cur_image_index].get(); }

	VkSwapchainKHR GetVkSwapchain() const { return m_swapchain; }
	uint32_t GetCurrentImageIndex() const { return m_cur_image_index; }

	static SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

private:

	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) const;
	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& present_modes) const;
	VkExtent2D ChooseSwapExtent(void* window_handle, const VkSurfaceCapabilitiesKHR& capabilities) const;

	void Init();
	void Cleanup();
};