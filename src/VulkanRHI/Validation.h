#pragma once

#include "StdAfx.h"

class VulkanValidationCallback
{
private:
	VkInstance m_vk_instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT m_vk_dbg_messenger = VK_NULL_HANDLE;

public:
	VulkanValidationCallback(VkInstance instance); // lifetime of the VkInstance must exceed the lifetime of this object
	~VulkanValidationCallback();
};