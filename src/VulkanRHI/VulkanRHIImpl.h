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

class VulkanRHI : public RHI
{
private:

	VkInstance m_vk_instance = VK_NULL_HANDLE;

	std::unique_ptr<class VulkanValidationCallback> m_validation_callback = nullptr;

	VkSurfaceKHR m_main_surface = VK_NULL_HANDLE;

	VkPhysicalDevice m_vk_phys_device = VK_NULL_HANDLE;

	std::vector<const char*> m_required_device_extensions =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};


public:
	VulkanRHI(const VulkanRHICreateInfo& info);
	virtual ~VulkanRHI() override;

	virtual void* GetNativeInstance() const { return (void*)&m_vk_instance; }
	virtual void* GetNativePhysDevice() const { return (void*)&m_vk_phys_device; }
	virtual void* GetNativeSurface() const { return (void*)&m_main_surface; }

private:
	void CreateVkInstance(const VulkanRHICreateInfo& info);

	void LogSupportedVkInstanceExtensions() const;

	std::vector<const char*> GetRequiredExtensions(bool enable_validation_layers) const;
	std::vector<const char*> GetSupportedLayers(const std::vector<const char*> wanted_layers) const;

	void PickPhysicalDevice(VkSurfaceKHR surface);
	bool IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) const;

	bool CheckDeviceExtensionSupport(VkPhysicalDevice device) const;
	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) const;
};
