#include "VulkanRHI.h"

#include <RHI/RHI.h>

class VulkanRHI : public RHI
{
public:
	VulkanRHI() {}
	virtual ~VulkanRHI() override {}
};

VULKANRHI_API RHI* CreateVulkanRHI()
{
	return new VulkanRHI();
}

VULKANRHI_API void DestroyVulkanRHI(RHI* vulkan_rhi)
{
	if (vulkan_rhi)
		delete vulkan_rhi;
}
