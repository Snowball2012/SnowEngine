#pragma once

#ifdef VULKANRHI_EXPORTS
#define VULKANRHI_API __declspec(dllexport)
#else
#define VULKANRHI_API __declspec(dllimport)
#endif

#include <RHI/RHI.h>

#include <span>

VULKANRHI_API class RHI* CreateVulkanRHI(std::span<const char*> external_extensions);

VULKANRHI_API void DestroyVulkanRHI(RHI* vulkan_rhi);

inline RHIPtr CreateVulkanRHI_RAII(std::span<const char*> external_extensions)
{
	return RHIPtr(CreateVulkanRHI(external_extensions), DestroyVulkanRHI);
}