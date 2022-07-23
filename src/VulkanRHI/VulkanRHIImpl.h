#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

#include "VulkanRHI.h"

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

class VulkanSwapChain : public SwapChain
{
private:
	VkSurfaceKHR m_surface = VK_NULL_HANDLE;
	VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
	std::vector<VkImage> m_swapchain_images;
	std::vector<VkImageView> m_swapchain_image_views;
	VkExtent2D m_swapchain_size_pixels = { 0, 0 };
	VkSurfaceFormatKHR m_swapchain_format = {};

	VulkanRHI* m_rhi = nullptr;

public:

	VulkanSwapChain(class VulkanRHI* rhi, const VulkanSwapChainCreateInfo& create_info);

	virtual ~VulkanSwapChain() override;

	// Inherited via SwapChain
	virtual void AddRef() override;
	virtual void Release() override;
	virtual glm::uvec2 GetExtent() const override;

	virtual void* GetNativeFormat() const override { return (void*)&m_swapchain_format.format; }

	static SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

private:

	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) const;
	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& present_modes) const;
	VkExtent2D ChooseSwapExtent(void* window_handle, const VkSurfaceCapabilitiesKHR& capabilities) const;
};


struct QueueFamilyIndices
{
	std::optional<uint32_t> graphics;
	std::optional<uint32_t> present;

	bool IsComplete() const { return graphics.has_value() && present.has_value(); }
};

QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	QueueFamilyIndices indices;

	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

	std::vector<VkQueueFamilyProperties> families(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, families.data());

	for (uint32_t i = 0; i < queue_family_count; ++i)
	{
		if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			indices.graphics = i;

		VkBool32 present_support = false;
		VK_VERIFY(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support));
		if (present_support)
			indices.present = i;

		if (indices.IsComplete())
			break;
	}

	return indices;
}

using VulkanSwapChainPtr = boost::intrusive_ptr<VulkanSwapChain>;

class VulkanRHI : public RHI
{
private:
	VkInstance m_vk_instance = VK_NULL_HANDLE;

	IVulkanWindowInterface* m_window_iface = nullptr;

	std::unique_ptr<class VulkanValidationCallback> m_validation_callback = nullptr;

	VkSurfaceKHR m_main_surface = VK_NULL_HANDLE;

	VkPhysicalDevice m_vk_phys_device = VK_NULL_HANDLE;

	std::vector<const char*> m_required_device_extensions =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	VkDevice m_vk_device = VK_NULL_HANDLE;

	QueueFamilyIndices m_queue_family_indices;

	VkQueue m_graphics_queue = VK_NULL_HANDLE;
	VkQueue m_present_queue = VK_NULL_HANDLE;

	VulkanSwapChainPtr m_main_swap_chain = nullptr;

	VkCommandPool m_cmd_pool = VK_NULL_HANDLE;

public:
	VulkanRHI(const VulkanRHICreateInfo& info);
	virtual ~VulkanRHI() override;

	virtual SwapChain* GetMainSwapChain() override { return m_main_swap_chain.get(); }

	// TEMP
	virtual void* GetNativePhysDevice() const override { return (void*)&m_vk_phys_device; }
	virtual void* GetNativeSurface() const override { return (void*)&m_main_surface; }
	virtual void* GetNativeDevice() const override { return (void*)&m_vk_device; }
	virtual void* GetNativeGraphicsQueue() const override { return (void*)&m_graphics_queue; }
	virtual void* GetNativePresentQueue() const override { return (void*)&m_present_queue; }

	VkPhysicalDevice GetPhysDevice() const { return m_vk_phys_device; }
	VkDevice GetDevice() const { return m_vk_device; }
	const QueueFamilyIndices& GetQueueFamilyIndices() const { return m_queue_family_indices; }
	IVulkanWindowInterface* GetWindowIface() const { return m_window_iface; }

	VkImageView CreateImageView(VkImage image, VkFormat format);
	void TransitionImageLayoutAndFlush(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout);

private:
	void CreateVkInstance(const VulkanRHICreateInfo& info);

	void LogSupportedVkInstanceExtensions() const;

	std::vector<const char*> GetRequiredExtensions(bool enable_validation_layers) const;
	std::vector<const char*> GetSupportedLayers(const std::vector<const char*> wanted_layers) const;

	void PickPhysicalDevice(VkSurfaceKHR surface);
	bool IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) const;

	bool CheckDeviceExtensionSupport(VkPhysicalDevice device) const;

	void CreateLogicalDevice();

	VulkanSwapChain* CreateSwapChainInternal(const VulkanSwapChainCreateInfo& create_info);

	void CreateCommandPool();

	VkCommandBuffer BeginSingleTimeCommands();
	void EndSingleTimeCommands(VkCommandBuffer buf);

	void TransitionImageLayout(VkCommandBuffer buf, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout);

};
