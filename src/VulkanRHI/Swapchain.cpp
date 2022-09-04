#include "StdAfx.h"

#include "Swapchain.h"

#include "VulkanRHIImpl.h"

IMPLEMENT_RHI_OBJECT(VulkanSwapChain)

VulkanSwapChain::VulkanSwapChain(VulkanRHI* rhi, const VulkanSwapChainCreateInfo& create_info)
    : m_rhi(rhi), m_surface(create_info.surface), m_create_info(create_info)
{
    Init();
}

void VulkanSwapChain::Init()
{
    VkPhysicalDevice phys_device = m_rhi->GetPhysDevice();
    auto swap_chain_support = QuerySwapChainSupport(phys_device, m_surface);

    m_swapchain_format = ChooseSwapSurfaceFormat(swap_chain_support.formats);
    VkPresentModeKHR present_mode = ChooseSwapPresentMode(swap_chain_support.present_modes);
    m_swapchain_size_pixels = ChooseSwapExtent(m_create_info.window_handle, swap_chain_support.capabilities);

    uint32_t image_count = swap_chain_support.capabilities.minImageCount + m_create_info.surface_num;

    if (swap_chain_support.capabilities.maxImageCount > 0 && image_count > swap_chain_support.capabilities.maxImageCount)
        image_count = swap_chain_support.capabilities.maxImageCount;

    std::cout << "Swap chain creation:\n";
    std::cout << "\twidth = " << m_swapchain_size_pixels.width << "\theight = " << m_swapchain_size_pixels.height << '\n';
    std::cout << "\tNum images = " << image_count << '\n';

    VkSwapchainCreateInfoKHR create_info_khr = {};
    create_info_khr.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info_khr.surface = m_surface;
    create_info_khr.minImageCount = image_count;
    create_info_khr.imageFormat = m_swapchain_format.format;
    create_info_khr.imageColorSpace = m_swapchain_format.colorSpace;
    create_info_khr.imageExtent = m_swapchain_size_pixels;
    create_info_khr.imageArrayLayers = 1;
    create_info_khr.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const QueueFamilyIndices& indices = m_rhi->GetQueueFamilyIndices();
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

    VkDevice vk_device = m_rhi->GetDevice();
    VK_VERIFY(vkCreateSwapchainKHR(vk_device, &create_info_khr, nullptr, &m_swapchain));

    VK_VERIFY(vkGetSwapchainImagesKHR(vk_device, m_swapchain, &image_count, nullptr));

    m_swapchain_images.clear();
    m_swapchain_images.reserve(image_count);
    std::vector<VkImage> images;
    images.resize(image_count);
    VK_VERIFY(vkGetSwapchainImagesKHR(vk_device, m_swapchain, &image_count, images.data()));

    RHI::TextureInfo tex_info = {};
    tex_info.dimensions = RHITextureDimensions::T2D;
    tex_info.depth = 1;
    tex_info.width = size_t(m_swapchain_size_pixels.width);
    tex_info.height = size_t(m_swapchain_size_pixels.height);
    tex_info.mips = 1;
    tex_info.format = VulkanRHI::GetRHIFormat(m_swapchain_format.format);
    tex_info.array_layers = 0;
    tex_info.usage = RHITextureUsageFlags::RTV;

    for (size_t i = 0; i < image_count; ++i)
    {
        m_swapchain_images.emplace_back(new VulkanTextureBase(images[i], tex_info));
    }

    m_swapchain_image_views.resize(image_count);
    for (uint32_t i = 0; i < image_count; ++i)
    {
        RHI::RTVInfo rtv_info = {};
        rtv_info.texture = m_swapchain_images[i].get();
        rtv_info.format = tex_info.format;

        m_swapchain_image_views[i] = RHIImpl(m_rhi->CreateRTV(rtv_info));
        m_rhi->DeferImageLayoutTransition(m_swapchain_images[i]->GetVkImage(), RHI::QueueType::Graphics, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        //m_rhi->TransitionImageLayoutAndFlush(m_swapchain_images[i].GetVkImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }
}


VulkanSwapChain::~VulkanSwapChain()
{
    Cleanup();
}

void VulkanSwapChain::Cleanup()
{
    m_swapchain_image_views.clear();

    vkDestroySwapchainKHR(m_rhi->GetDevice(), m_swapchain, nullptr);
}

glm::uvec2 VulkanSwapChain::GetExtent() const
{
    return glm::uvec2(m_swapchain_size_pixels.width, m_swapchain_size_pixels.height);
}

void VulkanSwapChain::AcquireNextImage(RHISemaphore* semaphore_to_signal, bool& out_recreated)
{
    VkSemaphore vk_semaphore = VK_NULL_HANDLE;
    if (VulkanSemaphore* semaphore_impl = static_cast<VulkanSemaphore*>(semaphore_to_signal))
        vk_semaphore = semaphore_impl->GetVkSemaphore();

    out_recreated = false;
    VkResult acquire_res = vkAcquireNextImageKHR(m_rhi->GetDevice(), m_swapchain, std::numeric_limits<uint64_t>::max(), vk_semaphore, VK_NULL_HANDLE, &m_cur_image_index);
    if (acquire_res == VK_ERROR_OUT_OF_DATE_KHR || acquire_res == VK_SUBOPTIMAL_KHR)
    {
        Recreate();
        out_recreated = true;
        acquire_res = vkAcquireNextImageKHR(m_rhi->GetDevice(), m_swapchain, std::numeric_limits<uint64_t>::max(), vk_semaphore, VK_NULL_HANDLE, &m_cur_image_index);
    }

    VK_VERIFY(acquire_res);
}

void VulkanSwapChain::Recreate()
{
    m_rhi->WaitIdle();

    Cleanup();
    Init();
}

RHIFormat VulkanSwapChain::GetFormat() const
{
    return VulkanRHI::GetRHIFormat(m_swapchain_format.format);
}

VkSurfaceFormatKHR VulkanSwapChain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) const
{
    for (const auto& format : available_formats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return format;
    }

    return available_formats[0];
}

VkPresentModeKHR VulkanSwapChain::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& present_modes) const
{
    for (const auto& present_mode : present_modes)
        if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
            return present_mode;

    return VK_PRESENT_MODE_FIFO_KHR; // guaranteed to be available by specs
}

VkExtent2D VulkanSwapChain::ChooseSwapExtent(void* window_handle, const VkSurfaceCapabilitiesKHR& capabilities) const
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return capabilities.currentExtent;

    VkExtent2D extent;
    m_rhi->GetWindowIface()->GetDrawableSize(window_handle, extent);

    extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return extent;
}

SwapChainSupportDetails VulkanSwapChain::QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
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
