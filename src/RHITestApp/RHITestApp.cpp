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

    const bool use_vulkan = true;
    if (use_vulkan)
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
    {
        // d3d12 path
        NOTIMPL;
    }

    m_swapchain = m_rhi->GetMainSwapChain();

    CreateDescriptorSetLayout();
    CreatePipeline();
    CreateSyncObjects();
    CreateTextureImage();
    CreateTextureImageView();
    CreateTextureSampler();
    CreateVertexBuffer();
    CreateIndexBuffer();
    CreateUniformBuffers();
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
    std::cout << "RHI shutdown started\n";

    m_binding_tables.clear();
    m_uniform_buffer_views.clear();
    m_uniform_buffers.clear();
    m_index_buffer = nullptr;
    m_vertex_buffer = nullptr;

    m_texture_sampler = nullptr;
    m_texture_srv = nullptr;
    m_texture = nullptr;

    m_swapchain = nullptr;
    CleanupPipeline();

    m_vertex_buffer = nullptr;
    m_index_buffer = nullptr;
    m_uniform_buffers.clear();
    m_texture = nullptr;
    m_image_available_semaphores.clear();
    m_render_finished_semaphores.clear();
    m_inflight_fences.clear();

    m_rhi.reset();

    std::cout << "RHI shutdown complete\n";
}

void RHITestApp::CleanupPipeline()
{
    m_rhi_graphics_pipeline = nullptr;
    m_binding_table_layout = nullptr;
    m_shader_bindings_layout = nullptr;
}

void RHITestApp::CopyBuffer(RHIBuffer& src, RHIBuffer& dst, size_t size)
{
    RHICommandList* list = BeginSingleTimeCommands();

    RHICommandList::CopyRegion copy_region = {};
    copy_region.src_offset = 0;
    copy_region.dst_offset = 0;
    copy_region.size = size;
    list->CopyBuffer(src, dst, 1, &copy_region);

    EndSingleTimeCommands(*list);
}

void RHITestApp::CreateIndexBuffer()
{
    const std::vector<uint16_t> indices =
    {
        0, 1, 2, 2, 3, 0
    };

    RHI::BufferInfo staging_info = {};

    staging_info.size = sizeof(indices[0]) * indices.size();
    staging_info.usage = RHIBufferUsageFlags::TransferSrc;

    RHIUploadBufferPtr staging_buf = m_rhi->CreateUploadBuffer(staging_info);
    VERIFY_NOT_EQUAL(staging_buf, nullptr);

    staging_buf->WriteBytes(indices.data(), staging_info.size, 0);

    RHI::BufferInfo buf_info = {};
    buf_info.size = staging_info.size;
    buf_info.usage = RHIBufferUsageFlags::IndexBuffer | RHIBufferUsageFlags::TransferDst;
    m_index_buffer = m_rhi->CreateDeviceBuffer(buf_info);

    CopyBuffer(*staging_buf->GetBuffer(), *m_index_buffer, buf_info.size);
}

void RHITestApp::CreateDescriptorSetLayout()
{
    RHI::DescriptorViewRange ranges[3] = {};
    ranges[0].type = RHIShaderBindingType::ConstantBuffer;
    ranges[0].count = 1;
    ranges[0].stages = RHIShaderStageFlags::VertexShader;

    ranges[1].type = RHIShaderBindingType::TextureRO;
    ranges[1].count = 1;
    ranges[1].stages = RHIShaderStageFlags::PixelShader;

    ranges[2].type = RHIShaderBindingType::Sampler;
    ranges[2].count = 1;
    ranges[2].stages = RHIShaderStageFlags::PixelShader;

    RHI::DescriptorSetLayoutInfo binding_table = {};
    binding_table.ranges = ranges;
    binding_table.range_count = std::size(ranges);

    m_binding_table_layout = m_rhi->CreateDescriptorSetLayout(binding_table);

    RHI::ShaderBindingLayoutInfo layout_info = {};
    RHIDescriptorSetLayout* dsls[1] = { m_binding_table_layout.get() };
    layout_info.tables = dsls;
    layout_info.table_count = std::size(dsls);

    m_shader_bindings_layout = m_rhi->CreateShaderBindingLayout(layout_info);
}

void RHITestApp::CreateUniformBuffers()
{
    m_uniform_buffers.resize(m_max_frames_in_flight);
    m_uniform_buffer_views.resize(m_max_frames_in_flight);

    RHI::BufferInfo uniform_info = {};
    uniform_info.size = sizeof(Matrices);
    uniform_info.usage = RHIBufferUsageFlags::UniformBuffer;

    for (size_t i = 0; i < m_max_frames_in_flight; ++i)
    {
        m_uniform_buffers[i] = m_rhi->CreateUploadBuffer(uniform_info);
        RHIBufferViewInfo view_info = {};
        view_info.buffer = m_uniform_buffers[i]->GetBuffer();
        m_uniform_buffer_views[i] = m_rhi->CreateUniformBufferView(view_info);
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

    m_uniform_buffers[current_image]->WriteBytes(&matrices, sizeof(matrices));
}

void RHITestApp::CopyBufferToImage(RHIBuffer& src, RHITexture& texture, uint32_t width, uint32_t height)
{
    RHICommandList* list = BeginSingleTimeCommands();

    RHIBufferTextureCopyRegion region = {};
    region.texture_subresource.mip_count = 1;
    region.texture_subresource.array_count = 1;
    region.texture_extent[0] = width;
    region.texture_extent[1] = height;
    region.texture_extent[2] = 1;

    list->CopyBufferToTexture(src, texture, &region, 1);

    EndSingleTimeCommands(*list);
}

void RHITestApp::CreateDescriptorSets()
{
    m_binding_tables.resize(m_max_frames_in_flight);

    for (size_t i = 0; i < m_max_frames_in_flight; ++i)
    {
        m_binding_tables[i] = m_rhi->CreateDescriptorSet(*m_binding_table_layout);
        m_binding_tables[i]->BindUniformBufferView(0, 0, *m_uniform_buffer_views[i]);
        m_binding_tables[i]->BindTextureROView(1, 0, *m_texture_srv);
        m_binding_tables[i]->BindSampler(2, 0, *m_texture_sampler);
        m_binding_tables[i]->FlushBinds(); // optional, will be flushed anyway on cmdlist.BindDescriptorSet
    }
}

void RHITestApp::CreateTextureImage()
{
    // upload to staging
    int width, height, channels;
    stbi_uc* pixels = stbi_load("textures/texture.jpg", &width, &height, &channels, STBI_rgb_alpha);

    VERIFY_NOT_EQUAL(pixels, nullptr);

    VkDeviceSize image_size = uint32_t(width) * uint32_t(height) * 4;

    RHI::BufferInfo staging_info = {};

    staging_info.size = image_size;
    staging_info.usage = RHIBufferUsageFlags::TransferSrc;

    RHIUploadBufferPtr staging_buf = m_rhi->CreateUploadBuffer(staging_info);

    VERIFY_NOT_EQUAL(staging_buf, nullptr);

    staging_buf->WriteBytes(pixels, staging_info.size, 0);

    stbi_image_free(pixels);

    // image creation
    CreateImage(
        uint32_t(width), uint32_t(height),
        RHIFormat::R8G8B8A8_SRGB,
        RHITextureUsageFlags::TextureROView | RHITextureUsageFlags::TransferDst,
        RHITextureLayout::TransferDst,
        m_texture);

    CopyBufferToImage(*staging_buf->GetBuffer(), *m_texture, uint32_t(width), uint32_t(height));

    TransitionImageLayoutAndFlush(
        *m_texture,
        RHITextureLayout::TransferDst,
        RHITextureLayout::ShaderReadOnly);
}

void RHITestApp::CreateImage(
    uint32_t width, uint32_t height, RHIFormat format, RHITextureUsageFlags usage, RHITextureLayout layout,
    RHITexturePtr& image)
{
    RHI::TextureInfo tex_info = {};
    tex_info.dimensions = RHITextureDimensions::T2D;
    tex_info.width = width;
    tex_info.height = height;
    tex_info.depth = 1;
    tex_info.mips = 1;
    tex_info.array_layers = 1;
    tex_info.format = format;
    tex_info.usage = usage;
    tex_info.initial_layout = layout;

    image = m_rhi->CreateTexture(tex_info);
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

void RHITestApp::TransitionImageLayoutAndFlush(RHITexture& texture, RHITextureLayout old_layout, RHITextureLayout new_layout)
{
    RHICommandList* cmd_list = BeginSingleTimeCommands();

    TransitionImageLayout(*cmd_list, texture, old_layout, new_layout);

    EndSingleTimeCommands(*cmd_list);
}

void RHITestApp::TransitionImageLayout(RHICommandList& cmd_list, RHITexture& texture, RHITextureLayout old_layout, RHITextureLayout new_layout)
{
    RHITextureBarrier barrier = {};
    barrier.layout_src = old_layout;
    barrier.layout_dst = new_layout;
    barrier.texture = &texture;
    barrier.subresources.mip_count = 1;
    barrier.subresources.array_count = 1;

    cmd_list.TextureBarriers(&barrier, 1);
}

void RHITestApp::CreateTextureImageView()
{
    RHI::TextureROViewInfo srv_info = {};
    srv_info.texture = m_texture.get();
    m_texture_srv = m_rhi->CreateTextureROView(srv_info);
}

void RHITestApp::CreateTextureSampler()
{
    RHI::SamplerInfo info = {};
    info.enable_anisotropy = true;
    m_texture_sampler = m_rhi->CreateSampler(info);
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

    size_t buffer_size = sizeof(vertices[0]) * vertices.size();

    RHI::BufferInfo staging_info = {};
    staging_info.size = buffer_size;
    staging_info.usage = RHIBufferUsageFlags::TransferSrc;

    RHIUploadBufferPtr staging_buf = m_rhi->CreateUploadBuffer(staging_info);

    VERIFY_NOT_EQUAL(staging_buf, nullptr);

    staging_buf->WriteBytes(vertices.data(), staging_info.size, 0);

    RHI::BufferInfo buf_info = {};
    buf_info.size = buffer_size;
    buf_info.usage = RHIBufferUsageFlags::VertexBuffer | RHIBufferUsageFlags::TransferDst;

    m_vertex_buffer = m_rhi->CreateDeviceBuffer(buf_info);

    CopyBuffer(*staging_buf->GetBuffer(), *m_vertex_buffer, buffer_size);
}

std::vector<const char*> RHITestApp::GetSDLExtensions() const
{
    uint32_t sdl_extension_count = 0;
    SDL_VERIFY(SDL_Vulkan_GetInstanceExtensions(m_main_wnd, &sdl_extension_count, nullptr));

    std::vector<const char*> sdl_extensions(sdl_extension_count);
    SDL_VERIFY(SDL_Vulkan_GetInstanceExtensions(m_main_wnd, &sdl_extension_count, sdl_extensions.data()));

    return sdl_extensions;
}

void RHITestApp::CreatePipeline()
{
    RHI::ShaderCreateInfo create_info = {};
    create_info.filename = L"shaders/helloworld.hlsl";

    create_info.frequency = RHI::ShaderFrequency::Vertex;
    create_info.entry_point = "TriangleVS";
    RHIObjectPtr<RHIShader> triangle_shader_vs = m_rhi->CreateShader(create_info);

    create_info.frequency = RHI::ShaderFrequency::Pixel;
    create_info.entry_point = "TrianglePS";
    RHIObjectPtr<RHIShader> triangle_shader_ps = m_rhi->CreateShader(create_info);

    auto vb_attributes = Vertex::GetRHIAttributes();

    RHIPrimitiveBufferLayout vb_layout = {};
    vb_layout.attributes = vb_attributes.data();
    vb_layout.attributes_count = vb_attributes.size();
    vb_layout.stride = sizeof(Vertex);

    const RHIPrimitiveBufferLayout* buffers[] = { &vb_layout };

    RHIPrimitiveFrequency frequencies[] = { RHIPrimitiveFrequency::PerVertex };

    static_assert(_countof(buffers) == _countof(frequencies));

    RHIInputAssemblerInfo ia_info = {};
    ia_info.buffers_count = _countof(buffers);
    ia_info.primitive_buffers = buffers;
    ia_info.frequencies = frequencies;

    RHIGraphicsPipelineInfo rhi_pipeline_info = {};

    rhi_pipeline_info.input_assembler = &ia_info;
    rhi_pipeline_info.vs = triangle_shader_vs.get();
    rhi_pipeline_info.ps = triangle_shader_ps.get();

    RHIPipelineRTInfo rt_info = {};
    rt_info.format = m_swapchain->GetFormat();
    rhi_pipeline_info.rts_count = 1;
    rhi_pipeline_info.rt_info = &rt_info;
    rhi_pipeline_info.binding_layout = m_shader_bindings_layout.get();

    m_rhi_graphics_pipeline = m_rhi->CreatePSO(rhi_pipeline_info);
}

void RHITestApp::RecordCommandBuffer(RHICommandList& list, RHISwapChain& swapchain)
{
    list.Begin();

    TransitionImageLayout(list, *swapchain.GetTexture(), RHITextureLayout::Present, RHITextureLayout::RenderTarget);

    RHIPassRTVInfo rt = {};
    rt.load_op = RHILoadOp::Clear;
    rt.store_op = RHIStoreOp::Store;
    rt.rtv = m_swapchain->GetRTV();
    rt.clear_value.float32[3] = 1.0f;

    RHIPassInfo pass_info = {};
    pass_info.render_area = RHIRect2D{ .offset = glm::ivec2(0,0), .extent = m_swapchain->GetExtent() };
    pass_info.render_targets = &rt;
    pass_info.render_targets_count = 1;
    list.BeginPass(pass_info);

    RHIViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    glm::uvec2 swapchain_extent = m_swapchain->GetExtent();
    viewport.width = float(swapchain_extent.x);
    viewport.height = float(swapchain_extent.y);
    viewport.min_depth = 0.0f;
    viewport.max_depth = 1.0f;

    RHIRect2D scissor = {};
    scissor.offset = { 0, 0 };
    scissor.extent = swapchain_extent;

    list.SetViewports(0, &viewport, 1);
    list.SetScissors(0, &scissor, 1);

    list.SetPSO(*m_rhi_graphics_pipeline);

    list.BindDescriptorSet(0, *m_binding_tables[m_current_frame]);
    
    list.SetVertexBuffers(0, m_vertex_buffer.get(), 1, nullptr);
    list.SetIndexBuffer(*m_index_buffer, RHIIndexBufferType::UInt16, 0);

    list.DrawIndexed(6, 1, 0, 0, 0);

    list.EndPass();

    TransitionImageLayout(list, *swapchain.GetTexture(), RHITextureLayout::RenderTarget, RHITextureLayout::Present);

    list.End();
}

void RHITestApp::DrawFrame()
{
    m_rhi->WaitForFenceCompletion(m_inflight_fences[m_current_frame]);
    
    if (m_fb_resized)
    {
        m_swapchain->Recreate();
        m_fb_resized = false;
    }
    bool swapchain_recreated = false;
    m_swapchain->AcquireNextImage(m_image_available_semaphores[m_current_frame].get(), swapchain_recreated);

    RHICommandList* cmd_list = m_rhi->GetCommandList(RHI::QueueType::Graphics);

    UpdateUniformBuffer(m_current_frame);

    RecordCommandBuffer(*cmd_list, *m_swapchain);

    RHI::SubmitInfo submit_info = {};
    submit_info.cmd_list_count = 1;
    submit_info.cmd_lists = &cmd_list;
    RHISemaphore* wait_semaphores[] = { m_image_available_semaphores[m_current_frame].get() };
    submit_info.semaphores_to_wait = wait_semaphores;
    submit_info.wait_semaphore_count = 1;
    submit_info.semaphore_to_signal = m_render_finished_semaphores[m_current_frame].get();
    RHIPipelineStageFlags stages_to_wait[] = { RHIPipelineStageFlags::ColorAttachmentOutput };
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
