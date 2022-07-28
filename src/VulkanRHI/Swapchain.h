#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

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
private:
	VkSurfaceKHR m_surface = VK_NULL_HANDLE;
	VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
	std::vector<VkImage> m_swapchain_images;
	std::vector<VkImageView> m_swapchain_image_views;
	VkExtent2D m_swapchain_size_pixels = { 0, 0 };
	VkSurfaceFormatKHR m_swapchain_format = {};

	const VulkanSwapChainCreateInfo m_create_info;

	class VulkanRHI* m_rhi = nullptr;

	uint32_t m_cur_image_index = -1;

public:

	VulkanSwapChain(class VulkanRHI* rhi, const VulkanSwapChainCreateInfo& create_info);

	virtual ~VulkanSwapChain() override;

	// Inherited via RHISwapChain
	virtual void AddRef() override;
	virtual void Release() override;
	virtual glm::uvec2 GetExtent() const override;
	virtual void AcquireNextImage(class RHISemaphore* semaphore_to_signal, bool& out_recreated) override;
	virtual void Recreate() override;

	virtual void* GetNativeFormat() const override { return (void*)&m_swapchain_format.format; }
	virtual void* GetNativeImage() const override { return (void*)&m_swapchain_images[m_cur_image_index]; }
	virtual void* GetNativeImageView() const override { return (void*)&m_swapchain_image_views[m_cur_image_index]; }

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