#include "StdAfx.h"

#include "VulkanRHIImpl.h"

#include "Validation.h"

VulkanRHI::VulkanRHI(const VulkanRHICreateInfo& info)
{
    CreateVkInstance(info);

    if (info.enable_validation)
        m_validation_callback.reset(new VulkanValidationCallback(m_vk_instance));

    VERIFY_NOT_EQUAL(info.main_window, nullptr);

    m_main_surface = info.main_window->CreateSurface(m_vk_instance);

    PickPhysicalDevice(m_main_surface);
}

VulkanRHI::~VulkanRHI()
{
    vkDestroySurfaceKHR(m_vk_instance, m_main_surface, nullptr);
    m_validation_callback.reset(); // we need to do this explicitly because VulkanValidationCallback requires valid VkInstance to destroy itself
    vkDestroyInstance(m_vk_instance, nullptr);
}

void VulkanRHI::CreateVkInstance(const VulkanRHICreateInfo& info)
{
    std::cout << "vkInstance creation started\n";

    LogSupportedVkInstanceExtensions();
    std::cout << std::endl;

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "hello vulkan";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    // extensions
    auto extensions = GetRequiredExtensions(info.enable_validation);
    extensions.insert(extensions.end(), info.required_external_extensions.begin(), info.required_external_extensions.end());

    create_info.enabledExtensionCount = uint32_t(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();

    // layers
    std::vector<const char*> wanted_layers = {};
    if (info.enable_validation)
        wanted_layers.push_back("VK_LAYER_KHRONOS_validation");

    std::cout << "Wanted validation layers:\n";
    for (const char* layer : wanted_layers)
    {
        std::cout << "\t" << layer << "\n";
    }
    std::cout << std::endl;

    std::vector<const char*> filtered_layers = GetSupportedLayers(wanted_layers);

    std::cout << std::endl;

    if (filtered_layers.size() < wanted_layers.size())
    {
        std::cerr << "Error: Some wanted vk instance layers are not available\n";
    }

    create_info.enabledLayerCount = uint32_t(filtered_layers.size());
    create_info.ppEnabledLayerNames = filtered_layers.data();

    VK_VERIFY(vkCreateInstance(&create_info, nullptr, &m_vk_instance));

    std::cout << "vkInstance creation complete\n";
}

void VulkanRHI::LogSupportedVkInstanceExtensions() const
{
    uint32_t extension_count = 0;
    VK_VERIFY(vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr));

    std::vector<VkExtensionProperties> extensions(extension_count);
    VK_VERIFY(vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions.data()));

    std::cout << "Available instance extensions:\n";

    for (const auto& extension : extensions)
    {
        std::cout << '\t' << extension.extensionName << "\tversion = " << extension.specVersion << '\n';
    }
}

std::vector<const char*> VulkanRHI::GetRequiredExtensions(bool enable_validation_layers) const
{
    std::vector<const char*> extensions = {};

    if (enable_validation_layers)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    return extensions;
}

std::vector<const char*> VulkanRHI::GetSupportedLayers(const std::vector<const char*> wanted_layers) const
{
    uint32_t layer_count = 0;
    VK_VERIFY(vkEnumerateInstanceLayerProperties(&layer_count, nullptr));

    std::vector<VkLayerProperties> available_layers(layer_count);
    VK_VERIFY(vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data()));

    // log available layers
    std::cout << "Available Vulkan Instance Layers:\n";
    for (const auto& layer : available_layers)
    {
        std::cout << '\t' << layer.layerName << "\tspec version = " << layer.specVersion << "\timplementation version = " << layer.implementationVersion << '\n';
    }

    std::vector<const char*> wanted_available_layers;
    for (const char* wanted_layer : wanted_layers)
    {
        if (available_layers.cend() !=
            std::find_if(available_layers.cbegin(), available_layers.cend(),
                [&](const auto& elem) { return !strcmp(wanted_layer, elem.layerName); }))
            wanted_available_layers.push_back(wanted_layer);
    }

    return wanted_available_layers;
}


namespace
{
    void LogDevice(VkPhysicalDevice device)
    {
        VkPhysicalDeviceProperties props;
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceProperties(device, &props);
        vkGetPhysicalDeviceFeatures(device, &features);

        std::cout << props.deviceName << "; Driver: " << props.driverVersion;
    }

    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphics;
        std::optional<uint32_t> present;

        bool IsComplete() const { return graphics.has_value() && present.has_value(); }
    };

    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        QueueFamilyIndices indices;

        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

        std::vector<VkQueueFamilyProperties> families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, families.data());

        for (uint32_t i = 0; i < queue_family_count; ++i)
        {
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                indices.graphics = i;

            VkBool32 present_support = false;
            VK_VERIFY(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support));
            if (present_support)
                indices.present = i;

            if (indices.IsComplete())
                break;
        }

        return indices;
    }
}

void VulkanRHI::PickPhysicalDevice(VkSurfaceKHR surface)
{
    uint32_t device_count = 0;
    VK_VERIFY(vkEnumeratePhysicalDevices(m_vk_instance, &device_count, nullptr));

    if (device_count == 0)
        throw std::runtime_error("Error: failed to find GPUs with Vulkan support");

    m_vk_phys_device = VK_NULL_HANDLE;

    std::vector<VkPhysicalDevice> devices(device_count);
    VK_VERIFY(vkEnumeratePhysicalDevices(m_vk_instance, &device_count, devices.data()));

    for (const auto& device : devices)
    {
        if (IsDeviceSuitable(device, surface))
        {
            m_vk_phys_device = device;
            break;
        }
    }

    if (!m_vk_phys_device)
        throw std::runtime_error("Error: failed to find a suitable GPU");

    std::cout << "Picked physical device:\n\t";
    LogDevice(m_vk_phys_device);
    std::cout << std::endl;
}

bool VulkanRHI::IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) const
{
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceVulkan13Features vk13_features = {};
    vk13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &vk13_features;

    vkGetPhysicalDeviceProperties(device, &props);
    vkGetPhysicalDeviceFeatures2(device, &features2);

    auto queue_families = FindQueueFamilies(device, surface);

    if (!queue_families.IsComplete())
        return false;

    if (!CheckDeviceExtensionSupport(device))
        return false;

    auto swap_chain_support = QuerySwapChainSupport(device, surface);

    if (swap_chain_support.formats.empty() || swap_chain_support.present_modes.empty())
        return false;

    if (!features2.features.samplerAnisotropy)
        return false;

    if (!vk13_features.dynamicRendering)
        return false;

    return true;
}

bool VulkanRHI::CheckDeviceExtensionSupport(VkPhysicalDevice device) const
{
    uint32_t extension_count = 0;
    VK_VERIFY(vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr));

    std::vector<VkExtensionProperties> available_extensions(extension_count);
    VK_VERIFY(vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data()));

    std::set<std::string> extensions_to_check(m_required_device_extensions.begin(), m_required_device_extensions.end());

    for (const auto& extension : available_extensions)
    {
        extensions_to_check.erase(extension.extensionName);
    }

    return extensions_to_check.empty();
}

SwapChainSupportDetails VulkanRHI::QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) const
{
    SwapChainSupportDetails details = {};

    VK_VERIFY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities));

    uint32_t format_count = 0;
    VK_VERIFY(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr));

    if (format_count > 0)
    {
        details.formats.resize(format_count);
        VK_VERIFY(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, details.formats.data()));
    }

    uint32_t present_mode_count = 0;
    VK_VERIFY(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr));

    if (present_mode_count > 0)
    {
        details.present_modes.resize(present_mode_count);
        VK_VERIFY(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, details.present_modes.data()));
    }

    return details;
}
