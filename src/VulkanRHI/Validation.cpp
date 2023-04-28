#include "StdAfx.h"

#include "Validation.h"

static uint32_t CalcHighestBit(uint32_t n)
{
    for (int i = 1; i < sizeof(n) * CHAR_BIT; i <<= 1)
        n |= n >> i;

    n = ((n + 1) >> 1) | (n & (1 << ((sizeof(n) * CHAR_BIT) - 1)));

    return n;
}

static const char* VkSeverityToString(VkDebugUtilsMessageSeverityFlagBitsEXT severity)
{
    switch (CalcHighestBit(severity))
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        return "Verbose";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        return "Info";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        return "Warning";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        return "Error";
    }
    return "Invalid Severity";
}

static VKAPI_ATTR VkBool32 VKAPI_CALL VkDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        SE_LOG_ERROR( VulkanRHI, "%s", callback_data->pMessage );

#ifndef NDEBUG
        DebugBreak();
#endif
    }

    return VK_FALSE;
}

VulkanValidationCallback::VulkanValidationCallback(VkInstance instance)
    : m_vk_instance(instance)
{
    VkDebugUtilsMessengerCreateInfoEXT create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT; // ignore INFO
    create_info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    create_info.pfnUserCallback = VkDebugCallback;
    create_info.pUserData = nullptr;

    auto vkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_vk_instance, "vkCreateDebugUtilsMessengerEXT"));
    if (vkCreateDebugUtilsMessengerEXT != nullptr)
    {
        VK_VERIFY(vkCreateDebugUtilsMessengerEXT(m_vk_instance, &create_info, nullptr, &m_vk_dbg_messenger));
    }
    else
    {
        throw std::runtime_error("Error: failed to find vkCreateDebugUtilsMessengerEXT address");
    }
}

VulkanValidationCallback::~VulkanValidationCallback()
{
    auto vkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_vk_instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (vkDestroyDebugUtilsMessengerEXT != nullptr)
    {
        vkDestroyDebugUtilsMessengerEXT(m_vk_instance, m_vk_dbg_messenger, nullptr);
    }
}