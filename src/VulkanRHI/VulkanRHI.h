#pragma once

#ifdef VULKANRHI_EXPORTS
#define VULKANRHI_API __declspec(dllexport)
#else
#define VULKANRHI_API __declspec(dllimport)
#endif

#include <RHI/RHI.h>

#include <span>

struct IVulkanWindowInterface
{
	virtual struct VkSurfaceKHR_T* CreateSurface(void* window_handle, struct VkInstance_T* instance) = 0; // use _T* to avoid including entire vulkan api here
	virtual void GetDrawableSize(void* window_handle, struct VkExtent2D& drawable_size) = 0;

	virtual ~IVulkanWindowInterface() {};
};

struct VulkanRHICreateInfo
{
	std::span<const char*> required_external_extensions;
	IVulkanWindowInterface* window_iface = nullptr;
	void* main_window_handle = nullptr;
	const char* app_name = nullptr;
	bool enable_validation = false;
	bool enable_raytracing = false;

	class Logger* logger = nullptr;
	struct CorePaths* core_paths = nullptr;
};

VULKANRHI_API class RHI* CreateVulkanRHI(const VulkanRHICreateInfo& info);

VULKANRHI_API void DestroyVulkanRHI(RHI* vulkan_rhi);

inline RHIPtr CreateVulkanRHI_RAII(const VulkanRHICreateInfo& info)
{
	return RHIPtr(CreateVulkanRHI(info), DestroyVulkanRHI);
}