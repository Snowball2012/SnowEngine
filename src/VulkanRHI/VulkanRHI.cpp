#include "VulkanRHI.h"

#include "VulkanRHIImpl.h"


VULKANRHI_API RHI* CreateVulkanRHI(std::span<const char*> external_extensions)
{
	return new VulkanRHI(external_extensions);
}

VULKANRHI_API void DestroyVulkanRHI(RHI* vulkan_rhi)
{
	if (vulkan_rhi)
		delete vulkan_rhi;
}
