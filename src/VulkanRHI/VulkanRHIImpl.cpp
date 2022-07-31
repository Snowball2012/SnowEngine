#include "StdAfx.h"

#include "VulkanRHIImpl.h"

#include "Validation.h"

#include "CommandLists.h"

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

    VulkanSwapChainCreateInfo swapchain_create_info = {};
    swapchain_create_info.surface = m_main_surface;
    swapchain_create_info.surface_num = 2;
    swapchain_create_info.window_handle = info.main_window_handle;

    m_main_swap_chain = CreateSwapChainInternal(swapchain_create_info);
}

VulkanRHI::~VulkanRHI()
{
    m_main_swap_chain = nullptr;

    vkDestroyCommandPool(m_vk_device, m_cmd_pool, nullptr);
    vkDestroyDevice(m_vk_device, nullptr);
    vkDestroySurfaceKHR(m_vk_instance, m_main_surface, nullptr);
    m_validation_callback.reset(); // we need to do this explicitly because VulkanValidationCallback requires valid VkInstance to destroy itself
    vkDestroyInstance(m_vk_instance, nullptr);
}

void VulkanRHI::WaitIdle()
{
    VK_VERIFY(vkDeviceWaitIdle(m_vk_device));
}

RHISemaphore* VulkanRHI::CreateGPUSemaphore()
{
    RHISemaphore* new_semaphore = new VulkanSemaphore(this);
    new_semaphore->AddRef();
    return new_semaphore;
}

void VulkanRHI::Present(RHISwapChain& swap_chain, const PresentInfo& info)
{
    boost::container::small_vector<VkSemaphore, 4> semaphores;
    if (info.wait_semaphores)
    {
        semaphores.resize(info.semaphore_count);
        for (size_t i = 0; i < info.semaphore_count; ++i)
            semaphores[i] = static_cast<VulkanSemaphore*>(info.wait_semaphores[i])->GetVkSemaphore();
    }

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    present_info.waitSemaphoreCount = uint32_t(semaphores.size());
    present_info.pWaitSemaphores = semaphores.data();

    auto vk_swap_chain = static_cast<VulkanSwapChain&>(swap_chain).GetVkSwapchain();
    uint32_t image_index = static_cast<VulkanSwapChain&>(swap_chain).GetCurrentImageIndex();
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &vk_swap_chain;
    present_info.pImageIndices = &image_index;

    present_info.pResults = nullptr;

    VK_VERIFY(vkQueuePresentKHR(m_present_queue, &present_info));
}

RHICommandList* VulkanRHI::GetCommandList(QueueType type)
{
    VERIFY_NOT_EQUAL(m_cmd_list_mgr, nullptr);

    return m_cmd_list_mgr->GetCommandList(type);
}

VkPipelineStageFlagBits VulkanRHI::GetVkStageFlags(PipelineStageFlags rhi_flags)
{
    uint64_t res_raw = 0;
    uint64_t checked_flags = 0;

    auto add_flag = [&res_raw, &checked_flags, rhi_flags](PipelineStageFlags rhi_flag, VkPipelineStageFlagBits vk_flag)
    {
        res_raw |= (uint64_t(rhi_flags) & uint64_t(rhi_flag)) ? vk_flag : 0;
        checked_flags |= uint64_t(rhi_flag);
    };

    add_flag(PipelineStageFlags::ColorAttachmentOutput, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

#ifndef NDEBUG
    VERIFY_EQUALS(checked_flags & uint64_t(rhi_flags), uint64_t(PipelineStageFlags::AllBits) & uint64_t(rhi_flags)); // some flags were not handled if this fires
#endif

    return VkPipelineStageFlagBits(res_raw);
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
            m_queue_family_indices = FindQueueFamilies(device, surface);
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

    auto swap_chain_support = VulkanSwapChain::QuerySwapChainSupport(device, surface);

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

void VulkanRHI::CreateLogicalDevice()
{
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<uint32_t> unique_queue_families = { m_queue_family_indices.graphics.value(), m_queue_family_indices.present.value() };


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

    vkGetDeviceQueue(m_vk_device, m_queue_family_indices.graphics.value(), 0, &m_graphics_queue);
    vkGetDeviceQueue(m_vk_device, m_queue_family_indices.present.value(), 0, &m_present_queue);

    if (!m_graphics_queue || !m_present_queue)
        throw std::runtime_error("Error: failed to get queues from logical device, but the device was created with it");

    std::cout << "Queues created successfully" << std::endl;
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
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = m_queue_family_indices.graphics.value();

    VK_VERIFY(vkCreateCommandPool(m_vk_device, &pool_info, nullptr, &m_cmd_pool));
}

VulkanSwapChain* VulkanRHI::CreateSwapChainInternal(const VulkanSwapChainCreateInfo& create_info)
{
    VulkanSwapChain* swap_chain = new VulkanSwapChain(this, create_info);
    swap_chain->AddRef();
    return swap_chain;
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

VulkanSemaphore::VulkanSemaphore(VulkanRHI* rhi)
    : m_rhi(rhi)
{
    VkSemaphoreCreateInfo sem_info = {};
    sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VK_VERIFY(vkCreateSemaphore(m_rhi->GetDevice(), &sem_info, nullptr, &m_vk_semaphore));
}

VulkanSemaphore::~VulkanSemaphore()
{
    if (m_vk_semaphore != VK_NULL_HANDLE)
        vkDestroySemaphore(m_rhi->GetDevice(), m_vk_semaphore, nullptr);
}

void VulkanSemaphore::AddRef()
{
}

void VulkanSemaphore::Release()
{
}
