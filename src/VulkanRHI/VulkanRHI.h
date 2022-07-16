#pragma once

#ifdef VULKANRHI_EXPORTS
#define VULKANRHI_API __declspec(dllexport)
#else
#define VULKANRHI_API __declspec(dllimport)
#endif

#include <RHI/RHI.h>

VULKANRHI_API class RHI* CreateVulkanRHI();

VULKANRHI_API void DestroyVulkanRHI(RHI* vulkan_rhi);

inline RHIPtr CreateVulkanRHI_RAII()
{
	return RHIPtr(CreateVulkanRHI(), DestroyVulkanRHI);
}