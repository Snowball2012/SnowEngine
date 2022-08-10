#include "StdAfx.h"

#include "RHITestApp.h"

#include <VulkanRHI/VulkanRHI.h>
#include <D3D12RHI/D3D12RHI.h>

RHITestApp::RHITestApp()
    : m_rhi(nullptr, [](auto*) {})
{
}

RHITestApp::~RHITestApp() = default;

void RHITestApp::Run()
{
    SDL_Init(SDL_INIT_EVERYTHING);
    const char* window_name = "hello vulkan";
    m_main_wnd = SDL_CreateWindow(window_name, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, m_window_size.x, m_window_size.y, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    InitRHI();
    MainLoop();
    Cleanup();

    SDL_DestroyWindow(m_main_wnd);
    m_main_wnd = nullptr;

    SDL_Quit();
}

struct SDLVulkanWindowInterface : public IVulkanWindowInterface
{
    virtual VkSurfaceKHR CreateSurface(void* window_handle, VkInstance instance) override
    {
        SDL_Window* sdl_handle = static_cast<SDL_Window*>(window_handle);
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        SDL_VERIFY(SDL_Vulkan_CreateSurface(sdl_handle, instance, &surface));
        return surface;
    }

    virtual void GetDrawableSize(void* window_handle, VkExtent2D& drawable_size)
    {
        SDL_Window* sdl_handle = static_cast<SDL_Window*>(window_handle);
        int w, h;
        SDL_Vulkan_GetDrawableSize(sdl_handle, &w, &h);
        VkExtent2D extent = { uint32_t(w), uint32_t(h) };
    }
};

void RHITestApp::InitRHI()
{
    std::cout << "RHI initialization started\n";

    if (true)
    {
        auto external_extensions = GetSDLExtensions();
        VulkanRHICreateInfo create_info;
        create_info.required_external_extensions = external_extensions;

    #ifdef NDEBUG
        create_info.enable_validation = false;
    #else
        create_info.enable_validation = true;
    #endif
        m_window_iface = std::make_unique<SDLVulkanWindowInterface>();

        create_info.window_iface = m_window_iface.get();
        create_info.main_window_handle = m_main_wnd;

        create_info.app_name = "SnowEngineRHITest";

        m_rhi = CreateVulkanRHI_RAII(create_info);
    }
    else
        m_rhi = CreateD3D12RHI_RAII();

    m_swapchain = m_rhi->GetMainSwapChain();
    // temp
    m_vk_phys_device = *static_cast<VkPhysicalDevice*>(m_rhi->GetNativePhysDevice());

    m_vk_device = *static_cast<VkDevice*>(m_rhi->GetNativeDevice());

    CreateDescriptorSetLayout();
    CreatePipeline();
    CreateSyncObjects();
    CreateTextureImage();
    CreateTextureImageView();
    CreateTextureSampler();
    CreateVertexBuffer();
    CreateIndexBuffer();
    CreateUniformBuffers();
    CreateDescriptorPool();
    CreateDescriptorSets();

    std::cout << "RHI initialization complete\n";
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
    m_rhi->WaitIdle();
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

    m_swapchain = nullptr;
    CleanupPipeline();
    vkDestroyDescriptorSetLayout(m_vk_device, m_descriptor_layout, nullptr);

    m_rhi.reset();

    std::cout << "RHI shutdown complete\n";
}


void RHITestApp::CleanupPipeline()
{
    vkDestroyPipeline(m_vk_device, m_graphics_pipeline, nullptr);
    vkDestroyPipelineLayout(m_vk_device, m_pipeline_layout, nullptr);

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
    RHICommandList* list = BeginSingleTimeCommands();
    VkCommandBuffer cmd_buf = *static_cast<VkCommandBuffer*>(list->GetNativeCmdList());;

    VkBufferCopy copy_region = {};
    copy_region.srcOffset = 0;
    copy_region.dstOffset = 0;
    copy_region.size = size;

    vkCmdCopyBuffer(cmd_buf, src, dst, 1, &copy_region);

    EndSingleTimeCommands(*list);
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

    glm::uvec2 swapchain_extent = m_swapchain->GetExtent();
    matrices.proj = glm::perspective(glm::radians(45.0f), float(swapchain_extent.x) / float(swapchain_extent.y), 0.1f, 10.0f);
    matrices.proj[1][1] *= -1; // ogl -> vulkan y axis

    void* data;
    VK_VERIFY(vkMapMemory(m_vk_device, m_ub_memory[current_image], 0, VK_WHOLE_SIZE, 0, &data));
    memcpy(data, &matrices, sizeof(matrices));
    vkUnmapMemory(m_vk_device, m_ub_memory[current_image]);

}

void RHITestApp::CopyBufferToImage(VkBuffer src, VkImage image, uint32_t width, uint32_t height)
{
    RHICommandList* list = BeginSingleTimeCommands();
    VkCommandBuffer cmd_buf = *static_cast<VkCommandBuffer*>(list->GetNativeCmdList());;

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

    EndSingleTimeCommands(*list);
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

RHICommandList* RHITestApp::BeginSingleTimeCommands()
{
    RHICommandList* list = m_rhi->GetCommandList(RHI::QueueType::Graphics);

    list->Begin();

    return list;
}

void RHITestApp::EndSingleTimeCommands(RHICommandList& list)
{
    list.End();

    RHI::SubmitInfo submit_info = {};
    submit_info.wait_semaphore_count = 0;
    submit_info.cmd_list_count = 1;
    RHICommandList* lists[] = { &list };
    submit_info.cmd_lists = lists;

    RHIFence completion_fence = m_rhi->SubmitCommandLists(submit_info);

    m_rhi->WaitForFenceCompletion(completion_fence);
}

void RHITestApp::TransitionImageLayoutAndFlush(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout)
{
    RHICommandList* cmd_list = BeginSingleTimeCommands();

    VkCommandBuffer buf = *static_cast<VkCommandBuffer*>(cmd_list->GetNativeCmdList());

    TransitionImageLayout(buf, image, old_layout, new_layout);

    EndSingleTimeCommands(*cmd_list);
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

std::vector<const char*> RHITestApp::GetSDLExtensions() const
{
    uint32_t sdl_extension_count = 0;
    SDL_VERIFY(SDL_Vulkan_GetInstanceExtensions(m_main_wnd, &sdl_extension_count, nullptr));

    std::vector<const char*> sdl_extensions(sdl_extension_count);
    SDL_VERIFY(SDL_Vulkan_GetInstanceExtensions(m_main_wnd, &sdl_extension_count, sdl_extensions.data()));

    return sdl_extensions;
}

namespace
{
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
    RHI::ShaderCreateInfo create_info = {};
    create_info.filename = L"shaders/helloworld.hlsl";

    create_info.frequency = RHI::ShaderFrequency::Vertex;
    create_info.entry_point = "TriangleVS";
    RHIShader* triangle_shader_vs = m_rhi->CreateShader(create_info);

    create_info.frequency = RHI::ShaderFrequency::Pixel;
    create_info.entry_point = "TrianglePS";
    RHIShader* triangle_shader_ps = m_rhi->CreateShader(create_info);

    VkPipelineShaderStageCreateInfo vs_stage_info = *static_cast<const VkPipelineShaderStageCreateInfo*>(triangle_shader_vs->GetNativeData());

    VkPipelineShaderStageCreateInfo ps_stage_info = *static_cast<const VkPipelineShaderStageCreateInfo*>(triangle_shader_ps->GetNativeData());;

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
    glm::uvec2 swapchain_extent = m_swapchain->GetExtent();
    viewport.width = float(swapchain_extent.x);
    viewport.height = float(swapchain_extent.y);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = { 0, 0 };
    scissor.extent = VkExtent2D(swapchain_extent.x, swapchain_extent.y);

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
    pipeline_rendering_info.pColorAttachmentFormats = (VkFormat*)m_swapchain->GetNativeFormat();
    
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

void RHITestApp::RecordCommandBuffer(VkCommandBuffer buf, VkImage swapchain_image, VkImageView swapchain_image_view)
{
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VK_VERIFY(vkBeginCommandBuffer(buf, &begin_info));

    TransitionImageLayout(buf, swapchain_image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingInfo render_info = {};
    render_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    render_info.colorAttachmentCount = 1;

    VkRenderingAttachmentInfo color_attachment = {};
    color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    VkClearValue clear_color = { {{0.0f,0.0f,0.0f,1.0f}} };
    color_attachment.clearValue = clear_color;
    color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment.imageView = swapchain_image_view;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    render_info.pColorAttachments = &color_attachment;
    render_info.renderArea = VkRect2D{ .offset = VkOffset2D{}, .extent = { m_swapchain->GetExtent().x, m_swapchain->GetExtent().y } };
    render_info.layerCount = 1;
    vkCmdBeginRendering(buf, &render_info);

    vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphics_pipeline);

    vkCmdBindDescriptorSets(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout, 0, 1, &m_desc_sets[m_current_frame], 0, nullptr);

    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(buf, 0, 1, &m_vertex_buffer, offsets);
    vkCmdBindIndexBuffer(buf, m_index_buffer, 0, VK_INDEX_TYPE_UINT16);

    vkCmdDrawIndexed(buf, 6, 1, 0, 0, 0);

    vkCmdEndRendering(buf);

    TransitionImageLayout(buf, swapchain_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_VERIFY(vkEndCommandBuffer(buf));
}

namespace
{
    VkSemaphore GetVkSemaphore(const RHISemaphore* semaphore)
    {
        return *static_cast<VkSemaphore*>(semaphore->GetNativeSemaphore());
    }
}

void RHITestApp::DrawFrame()
{
    m_rhi->WaitForFenceCompletion(m_inflight_fences[m_current_frame]);
    
    if (m_fb_resized)
    {
        RecreateSwapChain();
        m_fb_resized = false;
    }
    bool swapchain_recreated = false;
    m_swapchain->AcquireNextImage(m_image_available_semaphores[m_current_frame].get(), swapchain_recreated);
    if (swapchain_recreated)
    {
        CleanupPipeline();
        CreatePipeline();
    }

    RHICommandList* cmd_list = m_rhi->GetCommandList(RHI::QueueType::Graphics);

    VkCommandBuffer buf = *static_cast<VkCommandBuffer*>(cmd_list->GetNativeCmdList());

    VK_VERIFY(vkResetCommandBuffer(buf, 0));

    UpdateUniformBuffer(m_current_frame);

    VkImage swapchain_image = *static_cast<VkImage*>(m_swapchain->GetNativeImage());
    VkImageView swapchain_image_view = *static_cast<VkImageView*>(m_swapchain->GetNativeImageView());

    RecordCommandBuffer(buf, swapchain_image, swapchain_image_view);

    RHI::SubmitInfo submit_info = {};
    submit_info.cmd_list_count = 1;
    submit_info.cmd_lists = &cmd_list;
    RHISemaphore* wait_semaphores[] = { m_image_available_semaphores[m_current_frame].get() };
    submit_info.semaphores_to_wait = wait_semaphores;
    submit_info.wait_semaphore_count = 1;
    submit_info.semaphore_to_signal = m_render_finished_semaphores[m_current_frame].get();
    RHI::PipelineStageFlags stages_to_wait[] = { RHI::PipelineStageFlags::ColorAttachmentOutput };
    submit_info.stages_to_wait = stages_to_wait;

    m_inflight_fences[m_current_frame] = m_rhi->SubmitCommandLists(submit_info);

    RHI::PresentInfo present_info = {};
    present_info.semaphore_count = 1;
    RHISemaphore* wait_semaphore = m_render_finished_semaphores[m_current_frame].get();
    present_info.wait_semaphores = &wait_semaphore;

    m_rhi->Present(*m_swapchain, present_info);

    m_current_frame = (m_current_frame + 1) % m_max_frames_in_flight;
}

void RHITestApp::CreateSyncObjects()
{
    m_image_available_semaphores.resize(m_max_frames_in_flight, VK_NULL_HANDLE);
    m_render_finished_semaphores.resize(m_max_frames_in_flight, VK_NULL_HANDLE);
    m_inflight_fences.resize(m_max_frames_in_flight, {});

    for (size_t i = 0; i < m_max_frames_in_flight; ++i)
    {
        m_image_available_semaphores[i] = m_rhi->CreateGPUSemaphore();
        m_render_finished_semaphores[i] = m_rhi->CreateGPUSemaphore();
    }
}

void RHITestApp::RecreateSwapChain()
{
    m_swapchain->Recreate();
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
