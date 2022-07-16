#include "StdAfx.h"

#include "RHITestApp.h"

#include "Shader.h"

RHITestApp::RHITestApp()
{
#ifndef NDEBUG
    m_enable_validation_layer = true;
#endif
}

void RHITestApp::Run()
{
    SDL_Init(SDL_INIT_EVERYTHING);
    const char* window_name = "hello vulkan";
    m_main_wnd = SDL_CreateWindow(window_name, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, m_window_size.x, m_window_size.y, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    InitVulkan();
    MainLoop();
    Cleanup();

    SDL_DestroyWindow(m_main_wnd);
    m_main_wnd = nullptr;

    SDL_Quit();
}

uint32_t CalcHighestBit(uint32_t n)
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
        std::cerr
            << "[vkValidationLayer]"
            << ((type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) ? "[General]" : "")
            << ((type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) ? "[Validation]" : "")
            << ((type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) ? "[Performance]" : "")
            << "[" << VkSeverityToString(severity) << "]"
            << ": "
            << callback_data->pMessage << std::endl;

#ifndef NDEBUG
        DebugBreak();
#endif
    }

    return VK_FALSE;
}

void RHITestApp::InitVulkan()
{
    std::cout << "Vulkan initialization started\n";

    CreateVkInstance();
    SetupDebugMessenger();
    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateDescriptorSetLayout();
    CreateCommandPool();
    CreateSwapChain();
    CreatePipeline();
    CreateCommandBuffer();
    CreateSyncObjects();
    CreateTextureImage();
    CreateTextureImageView();
    CreateTextureSampler();
    CreateVertexBuffer();
    CreateIndexBuffer();
    CreateUniformBuffers();
    CreateDescriptorPool();
    CreateDescriptorSets();

    std::cout << "Vulkan initialization complete\n";
}

void RHITestApp::MainLoop()
{
    SDL_Event event;
    bool running = true;
    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                running = false;
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED)
            {
                m_fb_resized = true;
                int width = 0;
                int height = 0;
                SDL_GetWindowSize(m_main_wnd, &width, &height);
                m_window_size = glm::ivec2(width, height);
            }
        }
        DrawFrame();
    }
    VK_VERIFY(vkDeviceWaitIdle(m_vk_device));
}

void RHITestApp::Cleanup()
{
    std::cout << "Vulkan shutdown started\n";

    vkDestroyDescriptorPool(m_vk_device, m_desc_pool, nullptr);
    for (size_t i = 0; i < m_uniform_buffers.size(); ++i)
    {
        vkDestroyBuffer(m_vk_device, m_uniform_buffers[i], nullptr);
    }
    for (size_t i = 0; i < m_uniform_buffers.size(); ++i)
    {
        vkFreeMemory(m_vk_device, m_ub_memory[i], nullptr);
    }
    vkDestroyBuffer(m_vk_device, m_index_buffer, nullptr);
    vkFreeMemory(m_vk_device, m_ib_memory, nullptr);
    vkDestroyBuffer(m_vk_device, m_vertex_buffer, nullptr);
    vkFreeMemory(m_vk_device, m_vb_memory, nullptr);

    vkDestroySampler(m_vk_device, m_texture_sampler, nullptr);
    vkDestroyImageView(m_vk_device, m_texture_view, nullptr);
    vkDestroyImage(m_vk_device, m_texture_image, nullptr);
    vkFreeMemory(m_vk_device, m_texture_memory, nullptr);

    for (size_t i = 0; i < m_max_frames_in_flight; ++i)
    {
        vkDestroyFence(m_vk_device, m_inflight_fences[i], nullptr);
        vkDestroySemaphore(m_vk_device, m_render_finished_semaphores[i], nullptr);
        vkDestroySemaphore(m_vk_device, m_image_available_semaphores[i], nullptr);
    }

    vkDestroyCommandPool(m_vk_device, m_cmd_pool, nullptr);

    CleanupSwapChain();
    vkDestroyDescriptorSetLayout(m_vk_device, m_descriptor_layout, nullptr);

    vkDestroyDevice(m_vk_device, nullptr);
    vkDestroySurfaceKHR(m_vk_instance, m_surface, nullptr);
    DestroyDebugMessenger();
    vkDestroyInstance(m_vk_instance, nullptr);

    std::cout << "Vulkan shutdown complete\n";
}


void RHITestApp::CleanupSwapChain()
{
    vkDestroyPipeline(m_vk_device, m_graphics_pipeline, nullptr);
    vkDestroyPipelineLayout(m_vk_device, m_pipeline_layout, nullptr);

    for (auto& view : m_swapchain_image_views)
        vkDestroyImageView(m_vk_device, view, nullptr);

    vkDestroySwapchainKHR(m_vk_device, m_swapchain, nullptr);
}

void RHITestApp::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& buffer_mem)
{
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_VERIFY(vkCreateBuffer(m_vk_device, &buffer_info, nullptr, &buffer));

    VkMemoryRequirements mem_requirements = {};
    vkGetBufferMemoryRequirements(m_vk_device, buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = FindMemoryType(mem_requirements.memoryTypeBits, props);

    VK_VERIFY(vkAllocateMemory(m_vk_device, &alloc_info, nullptr, &buffer_mem));

    VK_VERIFY(vkBindBufferMemory(m_vk_device, buffer, buffer_mem, 0));
}

void RHITestApp::CopyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size)
{
    VkCommandBuffer cmd_buf = BeginSingleTimeCommands();

    VkBufferCopy copy_region = {};
    copy_region.srcOffset = 0;
    copy_region.dstOffset = 0;
    copy_region.size = size;

    vkCmdCopyBuffer(cmd_buf, src, dst, 1, &copy_region);

    EndSingleTimeCommands(cmd_buf);
}

void RHITestApp::CreateIndexBuffer()
{
    const std::vector<uint16_t> indices =
    {
        0, 1, 2, 2, 3, 0
    };

    VkDeviceSize buf_size = sizeof(indices[0]) * indices.size();

    VkBuffer staging_buf = VK_NULL_HANDLE;
    VkDeviceMemory staging_mem = VK_NULL_HANDLE;
    CreateBuffer(buf_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buf, staging_mem);

    void* data = nullptr;
    VK_VERIFY(vkMapMemory(m_vk_device, staging_mem, 0, VK_WHOLE_SIZE, 0, &data));
    memcpy(data, indices.data(), size_t(buf_size));
    vkUnmapMemory(m_vk_device, staging_mem);

    CreateBuffer(buf_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_index_buffer, m_ib_memory);

    CopyBuffer(staging_buf, m_index_buffer, buf_size);

    vkDestroyBuffer(m_vk_device, staging_buf, nullptr);
    vkFreeMemory(m_vk_device, staging_mem, nullptr);
}

void RHITestApp::CreateDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding mat_lb = {};
    mat_lb.binding = 0;
    mat_lb.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    mat_lb.descriptorCount = 1;
    mat_lb.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    mat_lb.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding sampler_lb = {};
    sampler_lb.binding = 1;
    sampler_lb.descriptorCount = 1;
    sampler_lb.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sampler_lb.pImmutableSamplers = nullptr;
    sampler_lb.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = { mat_lb, sampler_lb };
    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = uint32_t(bindings.size());
    layout_info.pBindings = bindings.data();

    VK_VERIFY(vkCreateDescriptorSetLayout(m_vk_device, &layout_info, nullptr, &m_descriptor_layout));
}

void RHITestApp::CreateUniformBuffers()
{
    VkDeviceSize buf_size = sizeof(Matrices);

    m_uniform_buffers.resize(m_max_frames_in_flight);
    m_ub_memory.resize(m_max_frames_in_flight);

    for (size_t i = 0; i < m_max_frames_in_flight; ++i)
    {
        CreateBuffer(
            buf_size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            m_uniform_buffers[i], m_ub_memory[i]);
    }
}

void RHITestApp::UpdateUniformBuffer(uint32_t current_image)
{
    static auto start_time = std::chrono::high_resolution_clock::now();

    auto current_time = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();

    Matrices matrices = {};
    matrices.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0, 0, 1));

    matrices.view = glm::lookAt(glm::vec3(2, 2, 2), glm::vec3(0, 0, 0), glm::vec3(0, 0, 1));

    matrices.proj = glm::perspective(glm::radians(45.0f), float(m_swapchain_size_pixels.width) / float(m_swapchain_size_pixels.height), 0.1f, 10.0f);
    matrices.proj[1][1] *= -1; // ogl -> vulkan y axis

    void* data;
    VK_VERIFY(vkMapMemory(m_vk_device, m_ub_memory[current_image], 0, VK_WHOLE_SIZE, 0, &data));
    memcpy(data, &matrices, sizeof(matrices));
    vkUnmapMemory(m_vk_device, m_ub_memory[current_image]);

}

void RHITestApp::CopyBufferToImage(VkBuffer src, VkImage image, uint32_t width, uint32_t height)
{
    VkCommandBuffer cmd_buf = BeginSingleTimeCommands();

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(
        cmd_buf, src, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    EndSingleTimeCommands(cmd_buf);
}

void RHITestApp::CreateDescriptorPool()
{
    std::array<VkDescriptorPoolSize, 2> pool_sizes = {};
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[0].descriptorCount = uint32_t(m_max_frames_in_flight);
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[1].descriptorCount = uint32_t(m_max_frames_in_flight);



    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = uint32_t(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = uint32_t(m_max_frames_in_flight);

    VK_VERIFY(vkCreateDescriptorPool(m_vk_device, &pool_info, nullptr, &m_desc_pool));
}

void RHITestApp::CreateDescriptorSets()
{
    std::vector<VkDescriptorSetLayout> layouts(m_max_frames_in_flight, m_descriptor_layout);

    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = m_desc_pool;
    alloc_info.descriptorSetCount = m_max_frames_in_flight;
    alloc_info.pSetLayouts = layouts.data();

    m_desc_sets.resize(m_max_frames_in_flight);
    VK_VERIFY(vkAllocateDescriptorSets(m_vk_device, &alloc_info, m_desc_sets.data()));

    for (size_t i = 0; i < m_max_frames_in_flight; ++i)
    {
        VkDescriptorBufferInfo buffer_info = {};
        buffer_info.buffer = m_uniform_buffers[i];
        buffer_info.offset = 0;
        buffer_info.range = VK_WHOLE_SIZE;

        VkDescriptorImageInfo image_info = {};
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info.imageView = m_texture_view;
        image_info.sampler = m_texture_sampler;

        std::array<VkWriteDescriptorSet, 2> desc_writes = {};
        desc_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        desc_writes[0].dstSet = m_desc_sets[i];
        desc_writes[0].dstBinding = 0;
        desc_writes[0].dstArrayElement = 0;
        desc_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_writes[0].descriptorCount = 1;
        desc_writes[0].pBufferInfo = &buffer_info;

        desc_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        desc_writes[1].dstSet = m_desc_sets[i];
        desc_writes[1].dstBinding = 1;
        desc_writes[1].dstArrayElement = 0;
        desc_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_writes[1].descriptorCount = 1;
        desc_writes[1].pImageInfo = &image_info;

        vkUpdateDescriptorSets(m_vk_device, uint32_t(desc_writes.size()), desc_writes.data(), 0, nullptr);
    }

}

void RHITestApp::CreateTextureImage()
{
    // upload to staging
    int width, height, channels;
    stbi_uc* pixels = stbi_load("textures/texture.jpg", &width, &height, &channels, STBI_rgb_alpha);

    VERIFY_NOT_EQUAL(pixels, nullptr);

    VkDeviceSize image_size = uint32_t(width) * uint32_t(height) * 4;
    VkBuffer staging_buffer;
    VkDeviceMemory staging_mem;

    CreateBuffer(
        image_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        staging_buffer,
        staging_mem);

    void* data;
    VK_VERIFY(vkMapMemory(m_vk_device, staging_mem, 0, VK_WHOLE_SIZE, 0, &data));
    memcpy(data, pixels, size_t(image_size));
    vkUnmapMemory(m_vk_device, staging_mem);

    stbi_image_free(pixels);

    // image creation
    CreateImage(
        uint32_t(width), uint32_t(height),
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_texture_image, m_texture_memory);

    TransitionImageLayoutAndFlush(
        m_texture_image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    CopyBufferToImage(staging_buffer, m_texture_image, uint32_t(width), uint32_t(height));

    TransitionImageLayoutAndFlush(
        m_texture_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(m_vk_device, staging_buffer, nullptr);
    vkFreeMemory(m_vk_device, staging_mem, nullptr);
}

void RHITestApp::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& image_mem)
{
    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;

    image_info.format = format;
    image_info.tiling = tiling;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    image_info.usage = usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.flags = 0;

    VK_VERIFY(vkCreateImage(m_vk_device, &image_info, nullptr, &image));

    VkMemoryRequirements mem_requirements = {};
    vkGetImageMemoryRequirements(m_vk_device, image, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = FindMemoryType(mem_requirements.memoryTypeBits, properties);

    VK_VERIFY(vkAllocateMemory(m_vk_device, &alloc_info, nullptr, &image_mem));

    VK_VERIFY(vkBindImageMemory(m_vk_device, image, image_mem, 0));
}

VkCommandBuffer RHITestApp::BeginSingleTimeCommands()
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

void RHITestApp::EndSingleTimeCommands(VkCommandBuffer buffer)
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

void RHITestApp::TransitionImageLayoutAndFlush(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout)
{
    VkCommandBuffer cmd_buf = BeginSingleTimeCommands();

    TransitionImageLayout(cmd_buf, image, old_layout, new_layout);

    EndSingleTimeCommands(cmd_buf);
}

void RHITestApp::TransitionImageLayout(VkCommandBuffer buf, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout)
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

void RHITestApp::CreateTextureImageView()
{
    m_texture_view = CreateImageView(m_texture_image, VK_FORMAT_R8G8B8A8_SRGB);

}

VkImageView RHITestApp::CreateImageView(VkImage image, VkFormat format)
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

void RHITestApp::CreateTextureSampler()
{
    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(m_vk_phys_device, &properties);

    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = std::min<float>(8.0f, properties.limits.maxSamplerAnisotropy);
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.mipLodBias = 0.0f;

    VK_VERIFY(vkCreateSampler(m_vk_device, &sampler_info, nullptr, &m_texture_sampler));
}

void RHITestApp::CreateVertexBuffer()
{
    std::vector<Vertex> vertices =
    {
        {{-0.5f, -0.5f}, {1, 1, 0}, {1, 0}},
        {{0.5f, -0.5f}, {0, 1, 0}, {0, 0}},
        {{0.5f, 0.5f}, {0, 1, 1}, {0, 1}},
        {{-0.5f, 0.5f}, {1, 0, 1}, {1, 1}}
    };

    VkDeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();

    VkBuffer staging_buf = VK_NULL_HANDLE;
    VkDeviceMemory staging_buf_mem = VK_NULL_HANDLE;
    CreateBuffer(
        buffer_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        staging_buf,
        staging_buf_mem);

    void* data;
    VK_VERIFY(vkMapMemory(m_vk_device, staging_buf_mem, 0, VK_WHOLE_SIZE, 0, &data));

    memcpy(data, vertices.data(), size_t(buffer_size));

    vkUnmapMemory(m_vk_device, staging_buf_mem);

    CreateBuffer(
        buffer_size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_vertex_buffer,
        m_vb_memory);

    CopyBuffer(staging_buf, m_vertex_buffer, buffer_size);

    vkDestroyBuffer(m_vk_device, staging_buf, nullptr);
    vkFreeMemory(m_vk_device, staging_buf_mem, nullptr);
}

uint32_t RHITestApp::FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties mem_properties = {};
    vkGetPhysicalDeviceMemoryProperties(m_vk_phys_device, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i)
    {
        if (type_filter & (1 << i) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }

    bool mem_type_found = false;
    VERIFY_EQUALS(mem_type_found, true);

    return uint32_t(-1);
}

void RHITestApp::CreateVkInstance()
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
    auto extensions = GetRequiredExtensions(m_enable_validation_layer);

    create_info.enabledExtensionCount = uint32_t(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();

    // layers
    std::vector<const char*> wanted_layers = {};
    if (m_enable_validation_layer)
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

void RHITestApp::SetupDebugMessenger()
{
    if (!m_enable_validation_layer)
    {
        std::cout << "Vulkan validation layer disabled\n";
        return;
    }

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

void RHITestApp::DestroyDebugMessenger()
{
    if (!m_enable_validation_layer)
        return;

    auto vkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_vk_instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (vkDestroyDebugUtilsMessengerEXT != nullptr)
    {
        vkDestroyDebugUtilsMessengerEXT(m_vk_instance, m_vk_dbg_messenger, nullptr);
    }
    else
    {
        throw std::runtime_error("Error: failed to find vkDestroyDebugUtilsMessengerEXT address");
    }
}

std::vector<const char*> RHITestApp::GetSDLExtensions() const
{
    uint32_t sdl_extension_count = 0;
    SDL_VERIFY(SDL_Vulkan_GetInstanceExtensions(m_main_wnd, &sdl_extension_count, nullptr));

    std::vector<const char*> sdl_extensions(sdl_extension_count);
    SDL_VERIFY(SDL_Vulkan_GetInstanceExtensions(m_main_wnd, &sdl_extension_count, sdl_extensions.data()));

    return sdl_extensions;
}

std::vector<const char*> RHITestApp::GetRequiredExtensions(bool enable_validation_layers) const
{
    auto extensions = GetSDLExtensions();

    if (enable_validation_layers)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    return extensions;
}

std::vector<const char*> RHITestApp::GetSupportedLayers(const std::vector<const char*> wanted_layers) const
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

void RHITestApp::LogSupportedVkInstanceExtensions() const
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

bool RHITestApp::CheckDeviceExtensionSupport(VkPhysicalDevice device) const
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

bool RHITestApp::IsDeviceSuitable(VkPhysicalDevice device) const
{
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceVulkan13Features vk13_features = {};
    vk13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &vk13_features;
    
    vkGetPhysicalDeviceProperties(device, &props);
    vkGetPhysicalDeviceFeatures2(device, &features2);

    auto queue_families = FindQueueFamilies(device, m_surface);

    if (!queue_families.IsComplete())
        return false;

    if (!CheckDeviceExtensionSupport(device))
        return false;

    auto swap_chain_support = QuerySwapChainSupport(device);

    if (swap_chain_support.formats.empty() || swap_chain_support.present_modes.empty())
        return false;

    if (!features2.features.samplerAnisotropy)
        return false;

    if (!vk13_features.dynamicRendering)
        return false;

    return true;
}

SwapChainSupportDetails RHITestApp::QuerySwapChainSupport(VkPhysicalDevice device) const
{
    SwapChainSupportDetails details = {};

    VK_VERIFY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities));

    uint32_t format_count = 0;
    VK_VERIFY(vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, nullptr));

    if (format_count > 0)
    {
        details.formats.resize(format_count);
        VK_VERIFY(vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, details.formats.data()));
    }

    uint32_t present_mode_count = 0;
    VK_VERIFY(vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, nullptr));

    if (present_mode_count > 0)
    {
        details.present_modes.resize(present_mode_count);
        VK_VERIFY(vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, details.present_modes.data()));
    }

    return details;
}

VkSurfaceFormatKHR RHITestApp::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) const
{
    for (const auto& format : available_formats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return format;
    }

    return available_formats[0];
}

VkPresentModeKHR RHITestApp::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& present_modes) const
{
    for (const auto& present_mode : present_modes)
        if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
            return present_mode;

    return VK_PRESENT_MODE_FIFO_KHR; // guaranteed to be available by specs
}

VkExtent2D RHITestApp::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return capabilities.currentExtent;

    int w, h;
    SDL_Vulkan_GetDrawableSize(m_main_wnd, &w, &h);
    VkExtent2D extent = { uint32_t(w), uint32_t(h) };

    extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return extent;
}

void RHITestApp::CreateSwapChain()
{
    auto swap_chain_support = QuerySwapChainSupport(m_vk_phys_device);

    m_swapchain_format = ChooseSwapSurfaceFormat(swap_chain_support.formats);
    VkPresentModeKHR present_mode = ChooseSwapPresentMode(swap_chain_support.present_modes);
    m_swapchain_size_pixels = ChooseSwapExtent(swap_chain_support.capabilities);

    uint32_t image_count = swap_chain_support.capabilities.minImageCount + m_max_frames_in_flight;

    if (swap_chain_support.capabilities.maxImageCount > 0 && image_count > swap_chain_support.capabilities.maxImageCount)
        image_count = swap_chain_support.capabilities.maxImageCount;

    std::cout << "Swap chain creation:\n";
    std::cout << "\twidth = " << m_swapchain_size_pixels.width << "\theight = " << m_swapchain_size_pixels.height << '\n';
    std::cout << "\tNum images = " << image_count << '\n';

    VkSwapchainCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = m_surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = m_swapchain_format.format;
    create_info.imageColorSpace = m_swapchain_format.colorSpace;
    create_info.imageExtent = m_swapchain_size_pixels;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = FindQueueFamilies(m_vk_phys_device, m_surface);
    uint32_t queue_family_indices[] = { indices.graphics.value(), indices.present.value() };

    if (queue_family_indices[0] != queue_family_indices[1])
    {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    }
    else
    {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices = nullptr;
    }

    create_info.preTransform = swap_chain_support.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    VK_VERIFY(vkCreateSwapchainKHR(m_vk_device, &create_info, nullptr, &m_swapchain));

    VK_VERIFY(vkGetSwapchainImagesKHR(m_vk_device, m_swapchain, &image_count, nullptr));

    m_swapchain_images.resize(image_count);
    VK_VERIFY(vkGetSwapchainImagesKHR(m_vk_device, m_swapchain, &image_count, m_swapchain_images.data()));

    m_swapchain_image_views.resize(image_count);
    for (uint32_t i = 0; i < image_count; ++i)
    {
        m_swapchain_image_views[i] = CreateImageView(m_swapchain_images[i], m_swapchain_format.format);
        TransitionImageLayoutAndFlush(m_swapchain_images[i], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }
}

void RHITestApp::PickPhysicalDevice()
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
        if (IsDeviceSuitable(device))
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

void RHITestApp::CreateLogicalDevice()
{
    auto queue_families = FindQueueFamilies(m_vk_phys_device, m_surface);

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

void RHITestApp::CreateSurface()
{
    SDL_VERIFY(SDL_Vulkan_CreateSurface(m_main_wnd, m_vk_instance, &m_surface));
}

namespace
{
    struct VkShaderModuleRAII
    {
        VkShaderModule sm = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;

        VkShaderModuleRAII(IDxcBlob& blob, VkDevice in_device)
            : device(in_device)
        {
            VkShaderModuleCreateInfo create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            create_info.codeSize = blob.GetBufferSize();
            create_info.pCode = reinterpret_cast<const uint32_t*>(blob.GetBufferPointer());

            VK_VERIFY(vkCreateShaderModule(device, &create_info, nullptr, &sm));
        }

        ~VkShaderModuleRAII()
        {
            if (sm != VK_NULL_HANDLE)
                vkDestroyShaderModule(device, sm, nullptr);
        }
    };

    std::string StringFromWchar(const wchar_t* wstr)
    {
        // inefficient
        std::string retval(size_t(lstrlenW(wstr)), '\0');
        char* dst = retval.data();
        while (wchar_t c = *wstr++)
        {
            *dst++ = char(c);
        }

        return retval;
    }
}

void RHITestApp::CreatePipeline()
{
    auto* compiler = ShaderCompiler::Get();
    if (!SE_ENSURE(compiler))
        throw std::runtime_error("failed to get shader compiler");

    auto shader_src = compiler->LoadSourceFile(L"shaders/helloworld.hlsl");
    VERIFY_NOT_EQUAL(shader_src, nullptr);

    Shader triangle_shader_vs(
        *shader_src, ShaderFrequency::Vertex, L"TriangleVS");
    Shader triangle_shader_ps(
        *shader_src, ShaderFrequency::Pixel, L"TrianglePS");

    auto vs_entrypoint = StringFromWchar(triangle_shader_vs.GetEntryPoint());
    auto ps_entrypoint = StringFromWchar(triangle_shader_ps.GetEntryPoint());

    IDxcBlob* result = triangle_shader_vs.GetNativeBytecode();
    VERIFY_NOT_EQUAL(result, nullptr);

    VkShaderModuleRAII vert_sm(*result, m_vk_device);

    result = triangle_shader_ps.GetNativeBytecode();
    VERIFY_NOT_EQUAL(result, nullptr);

    VkShaderModuleRAII pix_sm(*result, m_vk_device);

    VkPipelineShaderStageCreateInfo vs_stage_info = {};
    vs_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vs_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vs_stage_info.module = vert_sm.sm;
    vs_stage_info.pName = vs_entrypoint.c_str();

    VkPipelineShaderStageCreateInfo ps_stage_info = {};
    ps_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ps_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    ps_stage_info.module = pix_sm.sm;
    ps_stage_info.pName = ps_entrypoint.c_str();

    VkPipelineShaderStageCreateInfo stages[] = { vs_stage_info, ps_stage_info };

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
    auto binding_desc = Vertex::GetBindingDescription();
    std::array<VkVertexInputAttributeDescription, 3> attribute_desc = Vertex::GetAttributeDescriptions();

    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &binding_desc;
    vertex_input_info.vertexAttributeDescriptionCount = uint32_t(attribute_desc.size());
    vertex_input_info.pVertexAttributeDescriptions = attribute_desc.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = float(m_swapchain_size_pixels.width);
    viewport.height = float(m_swapchain_size_pixels.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = { 0, 0 };
    scissor.extent = m_swapchain_size_pixels;

    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState color_blend_attachment = {};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending = {};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    std::vector<VkDynamicState> dynamic_states =
    {
        VK_DYNAMIC_STATE_VIEWPORT
    };

    VkPipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.dynamicStateCount = uint32_t(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();

    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &m_descriptor_layout;
    pipeline_layout_info.pushConstantRangeCount = 0;

    VK_VERIFY(vkCreatePipelineLayout(m_vk_device, &pipeline_layout_info, nullptr, &m_pipeline_layout));

    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = stages;

    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = nullptr;

    VkPipelineRenderingCreateInfo pipeline_rendering_info = {};
    pipeline_rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipeline_rendering_info.colorAttachmentCount = 1;
    pipeline_rendering_info.pColorAttachmentFormats = &m_swapchain_format.format;
    
    pipeline_info.pNext = &pipeline_rendering_info;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = nullptr;

    pipeline_info.layout = m_pipeline_layout;
    pipeline_info.renderPass = nullptr;
    pipeline_info.subpass = 0;

    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    VK_VERIFY(vkCreateGraphicsPipelines(m_vk_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_graphics_pipeline));
}

void RHITestApp::CreateCommandPool()
{
    QueueFamilyIndices indices = FindQueueFamilies(m_vk_phys_device, m_surface);

    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = indices.graphics.value();

    VK_VERIFY(vkCreateCommandPool(m_vk_device, &pool_info, nullptr, &m_cmd_pool));
}

void RHITestApp::CreateCommandBuffer()
{
    m_cmd_buffers.resize(m_max_frames_in_flight, VK_NULL_HANDLE);

    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = m_cmd_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = m_max_frames_in_flight;

    VK_VERIFY(vkAllocateCommandBuffers(m_vk_device, &alloc_info, m_cmd_buffers.data()));
}

void RHITestApp::RecordCommandBuffer(VkCommandBuffer buf, uint32_t image_index)
{
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VK_VERIFY(vkBeginCommandBuffer(buf, &begin_info));

    TransitionImageLayout(buf, m_swapchain_images[image_index], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingInfo render_info = {};
    render_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    render_info.colorAttachmentCount = 1;

    VkRenderingAttachmentInfo color_attachment = {};
    color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    VkClearValue clear_color = { {{0.0f,0.0f,0.0f,1.0f}} };
    color_attachment.clearValue = clear_color;
    color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment.imageView = m_swapchain_image_views[image_index];
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    render_info.pColorAttachments = &color_attachment;
    render_info.renderArea = VkRect2D{ .offset = VkOffset2D{}, .extent = m_swapchain_size_pixels };
    render_info.layerCount = 1;
    vkCmdBeginRendering(buf, &render_info);

    vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphics_pipeline);

    vkCmdBindDescriptorSets(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout, 0, 1, &m_desc_sets[m_current_frame], 0, nullptr);

    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(buf, 0, 1, &m_vertex_buffer, offsets);
    vkCmdBindIndexBuffer(buf, m_index_buffer, 0, VK_INDEX_TYPE_UINT16);

    vkCmdDrawIndexed(buf, 6, 1, 0, 0, 0);

    vkCmdEndRendering(buf);

    TransitionImageLayout(buf, m_swapchain_images[image_index], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_VERIFY(vkEndCommandBuffer(buf));
}

void RHITestApp::DrawFrame()
{
    VK_VERIFY(vkWaitForFences(m_vk_device, 1, &m_inflight_fences[m_current_frame], VK_TRUE, std::numeric_limits<uint64_t>::max()));

    uint32_t image_index = uint32_t(-1);
    VkResult acquire_res = vkAcquireNextImageKHR(m_vk_device, m_swapchain, std::numeric_limits<uint64_t>::max(), m_image_available_semaphores[m_current_frame], VK_NULL_HANDLE, &image_index);
    if (acquire_res == VK_ERROR_OUT_OF_DATE_KHR || acquire_res == VK_SUBOPTIMAL_KHR || m_fb_resized)
    {
        m_fb_resized = false;
        RecreateSwapChain();
        acquire_res = vkAcquireNextImageKHR(m_vk_device, m_swapchain, std::numeric_limits<uint64_t>::max(), m_image_available_semaphores[m_current_frame], VK_NULL_HANDLE, &image_index);
    }

    VK_VERIFY(acquire_res);

    VK_VERIFY(vkResetFences(m_vk_device, 1, &m_inflight_fences[m_current_frame]));

    VK_VERIFY(vkResetCommandBuffer(m_cmd_buffers[m_current_frame], 0));

    UpdateUniformBuffer(m_current_frame);

    RecordCommandBuffer(m_cmd_buffers[m_current_frame], image_index);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore wait_semaphores[] = { m_image_available_semaphores[m_current_frame] };
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_cmd_buffers[m_current_frame];

    VkSemaphore signal_semaphores[] = { m_render_finished_semaphores[m_current_frame] };
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    VK_VERIFY(vkQueueSubmit(m_graphics_queue, 1, &submit_info, m_inflight_fences[m_current_frame]));

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    present_info.swapchainCount = 1;
    present_info.pSwapchains = &m_swapchain;
    present_info.pImageIndices = &image_index;

    present_info.pResults = nullptr;

    VK_VERIFY(vkQueuePresentKHR(m_present_queue, &present_info));

    m_current_frame = (m_current_frame + 1) % m_max_frames_in_flight;
}

void RHITestApp::CreateSyncObjects()
{
    m_image_available_semaphores.resize(m_max_frames_in_flight, VK_NULL_HANDLE);
    m_render_finished_semaphores.resize(m_max_frames_in_flight, VK_NULL_HANDLE);
    m_inflight_fences.resize(m_max_frames_in_flight, VK_NULL_HANDLE);

    VkSemaphoreCreateInfo sem_info = {};
    sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < m_max_frames_in_flight; ++i)
    {
        VK_VERIFY(vkCreateSemaphore(m_vk_device, &sem_info, nullptr, &m_image_available_semaphores[i]));
        VK_VERIFY(vkCreateSemaphore(m_vk_device, &sem_info, nullptr, &m_render_finished_semaphores[i]));
        VK_VERIFY(vkCreateFence(m_vk_device, &fence_info, nullptr, &m_inflight_fences[i]));
    }
}

void RHITestApp::RecreateSwapChain()
{
    VK_VERIFY(vkDeviceWaitIdle(m_vk_device));

    CleanupSwapChain();

    CreateSwapChain();
    CreatePipeline();
}

VkVertexInputBindingDescription Vertex::GetBindingDescription()
{
    VkVertexInputBindingDescription binding_desc = {};

    binding_desc.binding = 0;
    binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    binding_desc.stride = sizeof(Vertex);

    return binding_desc;
}

std::array<VkVertexInputAttributeDescription, 3> Vertex::GetAttributeDescriptions()
{
    std::array<VkVertexInputAttributeDescription, 3> attribute_descs = {};

    attribute_descs[0].binding = 0;
    attribute_descs[0].location = 0;
    attribute_descs[0].offset = offsetof(Vertex, pos);
    attribute_descs[0].format = VK_FORMAT_R32G32_SFLOAT;

    attribute_descs[1].binding = 0;
    attribute_descs[1].location = 1;
    attribute_descs[1].offset = offsetof(Vertex, color);
    attribute_descs[1].format = VK_FORMAT_R32G32B32_SFLOAT;

    attribute_descs[2].binding = 0;
    attribute_descs[2].location = 2;
    attribute_descs[2].offset = offsetof(Vertex, texcoord);
    attribute_descs[2].format = VK_FORMAT_R32G32_SFLOAT;


    return attribute_descs;
}
