#include "StdAfx.h"

#include "D3D12TestApp.h"

#include <VulkanRHI/VulkanRHI.h>
#include <D3D12RHI/D3D12RHI.h>

D3D12TestApp::D3D12TestApp()
    : m_rhi(nullptr, [](auto*) {})
{
}

D3D12TestApp::~D3D12TestApp() = default;

void D3D12TestApp::Run()
{
    SDL_Init(SDL_INIT_EVERYTHING);
    const char* window_name = "hello d3d12";
    m_main_wnd = SDL_CreateWindow(window_name, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, m_window_size.x, m_window_size.y, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    InitRHI();
    MainLoop();
    Cleanup();

    SDL_DestroyWindow(m_main_wnd);
    m_main_wnd = nullptr;

    SDL_Quit();
}

void D3D12TestApp::InitRHI()
{
    std::cout << "RHI initialization started\n";

    D3D12RHICreateInfo create_info = {};

#ifdef NDEBUG
    create_info.enable_validation = false;
#else
    create_info.enable_validation = true;
#endif

    HWND hwnd = nullptr;
    {
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        SDL_GetWindowWMInfo(m_main_wnd, &wmInfo);
        hwnd = wmInfo.info.win.window;
    }
    create_info.main_window_handle = hwnd;

    m_rhi = CreateD3D12RHI_RAII(create_info);

    m_swapchain = m_rhi->GetMainSwapChain();

    CreateSyncObjects();

    std::cout << "RHI initialization complete\n";
}

void D3D12TestApp::MainLoop()
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

void D3D12TestApp::Cleanup()
{
    std::cout << "RHI shutdown started\n";

    m_inflight_fences.clear();

    m_rhi.reset();

    std::cout << "RHI shutdown complete\n";
}

void D3D12TestApp::DrawFrame()
{
    m_rhi->WaitForFenceCompletion(m_inflight_fences[m_current_frame]);

    if (m_fb_resized)
    {
        // Recreate swapchain
        m_fb_resized = false;
    }

    RHICommandList* cmd_list = m_rhi->GetCommandList(RHI::QueueType::Graphics);

    cmd_list->Begin();

    cmd_list->End();

    RHI::SubmitInfo submit_info = {};
    submit_info.cmd_list_count = 1;
    submit_info.cmd_lists = &cmd_list;
    //submit_info.semaphore_to_signal = m_render_finished_semaphores[m_current_frame].get();
    RHIPipelineStageFlags stages_to_wait[] = { RHIPipelineStageFlags::ColorAttachmentOutput };
    submit_info.stages_to_wait = stages_to_wait;

    m_inflight_fences[m_current_frame] = m_rhi->SubmitCommandLists(submit_info);

    //RHI::PresentInfo present_info = {};
    //present_info.semaphore_count = 1;
    //RHISemaphore* wait_semaphore = m_render_finished_semaphores[m_current_frame].get();
    //present_info.wait_semaphores = &wait_semaphore;

    //m_rhi->Present(*m_swapchain, present_info);

    m_current_frame = (m_current_frame + 1) % m_max_frames_in_flight;
}

void D3D12TestApp::CreateSyncObjects()
{
    m_image_available_semaphores.resize(m_max_frames_in_flight, nullptr);
    m_render_finished_semaphores.resize(m_max_frames_in_flight, nullptr);
    m_inflight_fences.resize(m_max_frames_in_flight, {});

    //for (size_t i = 0; i < m_max_frames_in_flight; ++i)
    //{
    //    m_image_available_semaphores[i] = m_rhi->CreateGPUSemaphore();
    //    m_render_finished_semaphores[i] = m_rhi->CreateGPUSemaphore();
    //}
}
