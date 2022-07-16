#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

class VulkanRHI : public RHI
{
private:

	VkInstance m_vk_instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT m_vk_dbg_messenger = VK_NULL_HANDLE;
	bool m_enable_validation_layer = false;

public:
	VulkanRHI(std::span<const char*> required_external_extensions);
	virtual ~VulkanRHI() override;

private:
	void CreateVkInstance(std::span<const char*> required_external_extensions);

	void LogSupportedVkInstanceExtensions() const;

	std::vector<const char*> GetRequiredExtensions(bool enable_validation_layers) const;
	std::vector<const char*> GetSupportedLayers(const std::vector<const char*> wanted_layers) const;
};