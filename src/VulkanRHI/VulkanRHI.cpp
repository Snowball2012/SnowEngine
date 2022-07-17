#include "VulkanRHI.h"

#include "VulkanRHIImpl.h"


VULKANRHI_API RHI* CreateVulkanRHI(const VulkanRHICreateInfo& info)
{
	return new VulkanRHI(info);
}

VULKANRHI_API void DestroyVulkanRHI(RHI* vulkan_rhi)
{
	if (vulkan_rhi)
		delete vulkan_rhi;
}
