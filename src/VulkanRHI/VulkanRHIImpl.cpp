#include "StdAfx.h"

#include "VulkanRHIImpl.h"

#include "Validation.h"

VulkanRHI::VulkanRHI(const VulkanRHICreateInfo& info)
{
    CreateVkInstance(info);

    if (info.enable_validation)
        m_validation_callback.reset(new VulkanValidationCallback(m_vk_instance));

    VERIFY_NOT_EQUAL(info.main_window_handle, nullptr);
    VERIFY_NOT_EQUAL(info.window_iface, nullptr);

    m_window_iface = info.window_iface;

    m_main_surface = m_window_iface->CreateSurface(info.main_window_handle, m_vk_instance);

    PickPhysicalDevice(m_main_surface);

    CreateLogicalDevice();

    CreateCommandPool();
}

VulkanRHI::~VulkanRHI()
{
    vkDestroyCommandPool(m_vk_device, m_cmd_pool, nullptr);
    vkDestroyDevice(m_vk_device, nullptr);
    vkDestroySurfaceKHR(m_vk_instance, m_main_surface, nullptr);
    m_validation_callback.reset(); // we need to do this explicitly because VulkanValidationCallback requires valid VkInstance to destroy itself
    vkDestroyInstance(m_vk_instance, nullptr);
}

SwapChain* VulkanRHI::GetMainSwapChain()
{
    return nullptr;
}

void VulkanRHI::CreateVkInstance(const VulkanRHICreateInfo& info)
{
    std::cout << "vkInstance creation started\n";

    LogSupportedVkInstanceExtensions();
    std::cout << std::endl;

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = info.app_name;
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

void VulkanRHI::CreateLogicalDevice()
{
    auto queue_families = FindQueueFamilies(m_vk_phys_device, m_main_surface);

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<uint32_t> unique_queue_families = { queue_families.graphics.value(), queue_families.present.value() };


    float priority = 1.0f;
    for (uint32_t family : unique_queue_families)
    {
        auto& queue_create_info = queue_create_infos.emplace_back();
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = family;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &priority;
    }

    VkPhysicalDeviceVulkan13Features vk13_features = {};
    vk13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vk13_features.dynamicRendering = VK_TRUE;
    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &vk13_features;
    features2.features.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pQueueCreateInfos = queue_create_infos.data();
    device_create_info.queueCreateInfoCount = uint32_t(queue_create_infos.size());
    device_create_info.pEnabledFeatures = nullptr;
    device_create_info.enabledExtensionCount = uint32_t(m_required_device_extensions.size());
    device_create_info.ppEnabledExtensionNames = m_required_device_extensions.data();
    device_create_info.pNext = &features2;

    VK_VERIFY(vkCreateDevice(m_vk_phys_device, &device_create_info, nullptr, &m_vk_device));

    std::cout << "vkDevice created successfully" << std::endl;

    vkGetDeviceQueue(m_vk_device, queue_families.graphics.value(), 0, &m_graphics_queue);
    vkGetDeviceQueue(m_vk_device, queue_families.present.value(), 0, &m_present_queue);

    if (!m_graphics_queue || !m_present_queue)
        throw std::runtime_error("Error: failed to get queues from logical device, but the device was created with it");

    std::cout << "Queues created successfully" << std::endl;
}


VkSurfaceFormatKHR VulkanRHI::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) const
{
    for (const auto& format : available_formats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return format;
    }

    return available_formats[0];
}

VkPresentModeKHR VulkanRHI::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& present_modes) const
{
    for (const auto& present_mode : present_modes)
        if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
            return present_mode;

    return VK_PRESENT_MODE_FIFO_KHR; // guaranteed to be available by specs
}

VkExtent2D VulkanRHI::ChooseSwapExtent(void* window_handle, const VkSurfaceCapabilitiesKHR& capabilities) const
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return capabilities.currentExtent;

    VkExtent2D extent;
    m_window_iface->GetDrawableSize(window_handle, extent);

    extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return extent;
}

VkImageView VulkanRHI::CreateImageView(VkImage image, VkFormat format)
{
    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    view_info.components =
    {
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY
    };

    VkImageView view = VK_NULL_HANDLE;
    VK_VERIFY(vkCreateImageView(m_vk_device, &view_info, nullptr, &view));

    return view;
}

void VulkanRHI::CreateCommandPool()
{
    QueueFamilyIndices indices = FindQueueFamilies(m_vk_phys_device, m_main_surface);

    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = indices.graphics.value();

    VK_VERIFY(vkCreateCommandPool(m_vk_device, &pool_info, nullptr, &m_cmd_pool));
}


VulkanSwapChain* VulkanRHI::CreateSwapChainInternal(const VulkanSwapChainCreateInfo& create_info)
{
    auto swap_chain_support = QuerySwapChainSupport(m_vk_phys_device, create_info.surface);

    VkSurfaceFormatKHR swapchain_format = ChooseSwapSurfaceFormat(swap_chain_support.formats);
    VkPresentModeKHR present_mode = ChooseSwapPresentMode(swap_chain_support.present_modes);
    VkExtent2D swapchain_size_pixels = ChooseSwapExtent(create_info.window_handle, swap_chain_support.capabilities);

    uint32_t image_count = swap_chain_support.capabilities.minImageCount + create_info.surface_num;

    if (swap_chain_support.capabilities.maxImageCount > 0 && image_count > swap_chain_support.capabilities.maxImageCount)
        image_count = swap_chain_support.capabilities.maxImageCount;

    std::cout << "Swap chain creation:\n";
    std::cout << "\twidth = " << swapchain_size_pixels.width << "\theight = " << swapchain_size_pixels.height << '\n';
    std::cout << "\tNum images = " << image_count << '\n';

    VkSwapchainCreateInfoKHR create_info_khr = {};
    create_info_khr.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info_khr.surface = create_info.surface;
    create_info_khr.minImageCount = image_count;
    create_info_khr.imageFormat = swapchain_format.format;
    create_info_khr.imageColorSpace = swapchain_format.colorSpace;
    create_info_khr.imageExtent = swapchain_size_pixels;
    create_info_khr.imageArrayLayers = 1;
    create_info_khr.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = FindQueueFamilies(m_vk_phys_device, create_info.surface);
    uint32_t queue_family_indices[] = { indices.graphics.value(), indices.present.value() };

    if (queue_family_indices[0] != queue_family_indices[1])
    {
        create_info_khr.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info_khr.queueFamilyIndexCount = 2;
        create_info_khr.pQueueFamilyIndices = queue_family_indices;
    }
    else
    {
        create_info_khr.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info_khr.queueFamilyIndexCount = 0;
        create_info_khr.pQueueFamilyIndices = nullptr;
    }

    create_info_khr.preTransform = swap_chain_support.capabilities.currentTransform;
    create_info_khr.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info_khr.presentMode = present_mode;
    create_info_khr.clipped = VK_TRUE;
    create_info_khr.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VK_VERIFY(vkCreateSwapchainKHR(m_vk_device, &create_info_khr, nullptr, &swapchain));

    VK_VERIFY(vkGetSwapchainImagesKHR(m_vk_device, swapchain, &image_count, nullptr));

    std::vector<VkImage> swapchain_images;
    swapchain_images.resize(image_count);
    VK_VERIFY(vkGetSwapchainImagesKHR(m_vk_device, swapchain, &image_count, swapchain_images.data()));

    std::vector<VkImageView> swapchain_image_views;
    swapchain_image_views.resize(image_count);
    for (uint32_t i = 0; i < image_count; ++i)
    {
        swapchain_image_views[i] = CreateImageView(swapchain_images[i], swapchain_format.format);
        TransitionImageLayoutAndFlush(swapchain_images[i], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }

    VulkanSwapChain* retval = new VulkanSwapChain();
    retval->m_rhi = this;
    retval->m_surface = create_info.surface;
    retval->m_swapchain = swapchain;
    retval->m_swapchain_images = std::move(swapchain_images);
    retval->m_swapchain_image_views = std::move(swapchain_image_views);
    retval->m_swapchain_size_pixels = swapchain_size_pixels;
    retval->m_swapchain_format = swapchain_format;

    retval->AddRef();

    return retval;
}

VkCommandBuffer VulkanRHI::BeginSingleTimeCommands()
{
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = m_cmd_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer buf;
    VK_VERIFY(vkAllocateCommandBuffers(m_vk_device, &alloc_info, &buf));

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_VERIFY(vkBeginCommandBuffer(buf, &begin_info));

    return buf;
}

void VulkanRHI::EndSingleTimeCommands(VkCommandBuffer buffer)
{
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &buffer;

    VK_VERIFY(vkEndCommandBuffer(buffer));
    VK_VERIFY(vkQueueSubmit(m_graphics_queue, 1, &submit_info, VK_NULL_HANDLE));
    VK_VERIFY(vkQueueWaitIdle(m_graphics_queue));

    vkFreeCommandBuffers(m_vk_device, m_cmd_pool, 1, &buffer);
}

void VulkanRHI::TransitionImageLayoutAndFlush(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout)
{
    VkCommandBuffer cmd_buf = BeginSingleTimeCommands();

    TransitionImageLayout(cmd_buf, image, old_layout, new_layout);

    EndSingleTimeCommands(cmd_buf);
}

void VulkanRHI::TransitionImageLayout(VkCommandBuffer buf, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout)
{
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;

    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = 0;

    VkPipelineStageFlags src_stage = 0;
    VkPipelineStageFlags dst_stage = 0;

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
    {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = 0;

        src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR && new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }

    else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = 0;

        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }
    else
    {
        bool unsupported_layout_transition = true;
        VERIFY_EQUALS(unsupported_layout_transition, false);
    }

    vkCmdPipelineBarrier(
        buf,
        src_stage, dst_stage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);
}


VulkanSwapChain::~VulkanSwapChain()
{
}

void VulkanSwapChain::AddRef()
{
}

void VulkanSwapChain::Release()
{
}
