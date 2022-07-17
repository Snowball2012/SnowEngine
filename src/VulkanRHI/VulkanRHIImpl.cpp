#include "StdAfx.h"

#include "VulkanRHIImpl.h"

#include "Validation.h"

VulkanRHI::VulkanRHI(const VulkanRHICreateInfo& info)
{
    CreateVkInstance(info);

    if (info.enable_validation)
        m_validation_callback.reset(new VulkanValidationCallback(m_vk_instance));
}

VulkanRHI::~VulkanRHI()
{
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
