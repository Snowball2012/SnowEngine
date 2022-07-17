#pragma once

#ifdef VULKANRHI_EXPORTS
#define VULKANRHI_API __declspec(dllexport)
#else
#define VULKANRHI_API __declspec(dllimport)
#endif

#include <RHI/RHI.h>

#include <span>

struct IVulkanSurfaceFactory
{
	virtual struct VkSurfaceKHR_T* CreateSurface(struct VkInstance_T* instance) = 0; // use _T* to avoid including entire vulkan api here
	virtual ~IVulkanSurfaceFactory() {};
};

struct VulkanRHICreateInfo
{
	std::span<const char*> required_external_extensions;
	IVulkanSurfaceFactory* main_window;
	bool enable_validation = false;
};

VULKANRHI_API class RHI* CreateVulkanRHI(const VulkanRHICreateInfo& info);

VULKANRHI_API void DestroyVulkanRHI(RHI* vulkan_rhi);

inline RHIPtr CreateVulkanRHI_RAII(const VulkanRHICreateInfo& info)
{
	return RHIPtr(CreateVulkanRHI(info), DestroyVulkanRHI);
}