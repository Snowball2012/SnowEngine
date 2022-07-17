#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

#include "VulkanRHI.h"

class VulkanRHI : public RHI
{
private:

	VkInstance m_vk_instance = VK_NULL_HANDLE;

	std::unique_ptr<class VulkanValidationCallback> m_validation_callback = nullptr;

public:
	VulkanRHI(const VulkanRHICreateInfo& info);
	virtual ~VulkanRHI() override;

	virtual void* GetNativeInstance() const { return (void*)&m_vk_instance; }

private:
	void CreateVkInstance(const VulkanRHICreateInfo& info);

	void LogSupportedVkInstanceExtensions() const;

	std::vector<const char*> GetRequiredExtensions(bool enable_validation_layers) const;
	std::vector<const char*> GetSupportedLayers(const std::vector<const char*> wanted_layers) const;
};
