#include "VulkanRHI.h"

#include "VulkanRHIImpl.h"

CorePaths g_core_paths;
Logger* g_log;

ConsoleVariableBase* g_cvars_head = nullptr;

VULKANRHI_API RHI* CreateVulkanRHI(const VulkanRHICreateInfo& info)
{
	if ( !SE_ENSURE( info.logger ) )
		return nullptr;

	g_log = info.logger;

	if ( !SE_ENSURE( info.core_paths ) )
		return nullptr;

	g_core_paths = *info.core_paths;

	return new VulkanRHI(info);
}

VULKANRHI_API void DestroyVulkanRHI(RHI* vulkan_rhi)
{
	if (vulkan_rhi)
		delete vulkan_rhi;
}

VULKANRHI_API ConsoleVariableBase* VulkanRHI_GetCVarListHead()
{
	return g_cvars_head;
}