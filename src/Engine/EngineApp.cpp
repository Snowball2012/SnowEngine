#include "StdAfx.h"

#include "EngineApp.h"
#include "AssetManager.h"

#include "Rendergraph.h"

#include <VulkanRHI/VulkanRHI.h>
#include <D3D12RHI/D3D12RHI.h>

#include <ImguiBackend/ImguiBackend.h>

#include <vulkan/vulkan.h>

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities = {};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

EngineApp::EngineApp()
    : m_rhi( nullptr, []( auto* ) {} )
{
}

EngineApp::~EngineApp() = default;

void EngineApp::Run( int argc, char** argv )
{
    // Several systems have to start here

    // Logs
    // App console
    // RHI
    // Input
    // ImGui

    ParseCommandLine( argc, argv );

    InitCoreGlobals();

    SDL_Init( SDL_INIT_EVERYTHING );
    m_main_wnd = SDL_CreateWindow( GetMainWindowName(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, m_window_size.x, m_window_size.y, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    InitRHI();
    InitImGUI();

    m_asset_mgr = std::make_unique<AssetManager>();

    InitEngineGlobals();

    OnInit();

    MainLoop();

    OnCleanup();

    Cleanup();

    SDL_DestroyWindow( m_main_wnd );
    m_main_wnd = nullptr;

    SDL_Quit();
}

void EngineApp::ParseCommandLine( int argc, char** argv )
{
    for ( int i = 1; i < argc; ++i )
    {
        if ( !_strcmpi( argv[i], "-EnginePath" ) )
        {
            if ( argc > ( i + 1 ) )
            {
                // read next arg
                m_cmd_line_args.engine_content_path = argv[i + 1];
                m_cmd_line_args.engine_content_path += "/EngineContent/";
                i += 1;

                std::cout << "[App]: Set engine content path from command line: " << m_cmd_line_args.engine_content_path << std::endl;
            }
            else
            {
                std::cerr << "[Error][App]: Can't set engine content path from command line because -EnginePath is the last token" << std::endl;
            }
        }
        else if ( !_strcmpi( argv[i], "-LogPath" ) )
        {
            if ( argc > ( i + 1 ) )
            {
                // read next arg
                m_cmd_line_args.log_path = argv[i + 1];
                i += 1;

                std::cout << "[App]: Set log path from command line: " << m_cmd_line_args.log_path << std::endl;
            }
            else
            {
                std::cerr << "[Error][App]: Can't set log path from command line because -LogPath is the last token" << std::endl;
            }
        }
    }

    ParseCommandLineDerived( argc, argv );
}

struct SDLVulkanWindowInterface : public IVulkanWindowInterface
{
    virtual VkSurfaceKHR CreateSurface( void* window_handle, VkInstance instance ) override
    {
        SDL_Window* sdl_handle = static_cast< SDL_Window* >( window_handle );
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        SDL_VERIFY( SDL_Vulkan_CreateSurface( sdl_handle, instance, &surface ) );
        return surface;
    }

    virtual void GetDrawableSize( void* window_handle, VkExtent2D& drawable_size )
    {
        SDL_Window* sdl_handle = static_cast< SDL_Window* >( window_handle );
        int w, h;
        SDL_Vulkan_GetDrawableSize( sdl_handle, &w, &h );
        VkExtent2D extent = { uint32_t( w ), uint32_t( h ) };
    }
};

void EngineApp::InitRHI()
{
    SE_LOG_INFO( Engine, "RHI initialization started" );

    const bool use_vulkan = true;
    if ( use_vulkan )
    {
        auto external_extensions = GetSDLExtensions();
        VulkanRHICreateInfo create_info;
        create_info.required_external_extensions = external_extensions;

#ifdef NDEBUG
        create_info.enable_validation = false;
#else
        create_info.enable_validation = true;
#endif

        create_info.enable_raytracing = true;

        m_window_iface = std::make_unique<SDLVulkanWindowInterface>();

        create_info.window_iface = m_window_iface.get();
        create_info.main_window_handle = m_main_wnd;

        create_info.app_name = GetAppName();

        create_info.logger = g_log;
        create_info.core_paths = &g_core_paths;

        m_rhi = CreateVulkanRHI_RAII( create_info );
    }
    else
    {
        // d3d12 path
        NOTIMPL;
    }

    m_swapchain = m_rhi->GetMainSwapChain();

    CreateSyncObjects();

    SE_LOG_INFO( Engine, "RHI initialization complete" );
}

void EngineApp::MainLoop()
{
    SDL_Event event;
    bool running = true;
    while ( running )
    {
        while ( SDL_PollEvent( &event ) )
        {
            auto imgui_event_res = m_imgui->ProcessEvent( &event );

            if ( event.type == SDL_QUIT )
            {
                running = false;
            }
            if ( event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED )
            {
                m_fb_resized = true;
                int width = 0;
                int height = 0;
                SDL_GetWindowSize( m_main_wnd, &width, &height );
                m_window_size = glm::ivec2( width, height );
            }

            if ( !imgui_event_res.wantsCaptureKeyboard )
            {
                // forward keyboard event
            }
            if ( !imgui_event_res.wantsCaptureMouse )
            {
                // forward mouse event
            }
        }
        Update();
    }
    m_rhi->WaitIdle();
}

void EngineApp::Cleanup()
{
    SE_LOG_INFO( Engine, "RHI shutdown started" );

    m_imgui = nullptr;

    m_swapchain = nullptr;

    m_image_available_semaphores.clear();
    m_render_finished_semaphores.clear();
    m_inflight_fences.clear();

    g_engine.asset_mgr = nullptr;
    m_asset_mgr.reset();

    g_engine.rhi = nullptr;
    m_rhi.reset();

    SE_LOG_INFO( Engine, "RHI shutdown complete" );

    if ( g_log )
    {
        delete g_log;
        g_log = nullptr;
    }
}

void EngineApp::Update()
{
    OnUpdate();
    DrawFrame();
}

void EngineApp::InitImGUI()
{
    m_imgui = std::make_unique<ImguiBackend>( m_main_wnd, m_rhi.get(), m_swapchain->GetFormat() );
}

std::vector<const char*> EngineApp::GetSDLExtensions() const
{
    uint32_t sdl_extension_count = 0;
    SDL_VERIFY( SDL_Vulkan_GetInstanceExtensions( m_main_wnd, &sdl_extension_count, nullptr ) );

    std::vector<const char*> sdl_extensions( sdl_extension_count );
    SDL_VERIFY( SDL_Vulkan_GetInstanceExtensions( m_main_wnd, &sdl_extension_count, sdl_extensions.data() ) );

    return sdl_extensions;
}

void EngineApp::DrawFrame()
{
    m_rhi->WaitForFenceCompletion( m_inflight_fences[m_current_frame] );
    m_imgui->MarkFrameAsCompleted( m_imgui_frames[m_current_frame] );

    if ( m_fb_resized )
    {
        m_swapchain->Recreate();
        m_fb_resized = false;
        OnSwapChainRecreated();
    }
    bool swapchain_recreated = false;
    m_swapchain->AcquireNextImage( m_image_available_semaphores[m_current_frame].get(), swapchain_recreated );

    std::vector<RHICommandList*> cmd_lists;

    Rendergraph framegraph;

    OnDrawFrame( cmd_lists, framegraph );

    framegraph.Submit();

    ImguiRenderResult imgui_res = m_imgui->RenderFrame( *m_swapchain->GetRTV() );
    m_imgui_frames[m_current_frame] = imgui_res.frame_idx;

    if ( imgui_res.cl )
    {
        cmd_lists.emplace_back( imgui_res.cl );
    }

    RHI::SubmitInfo submit_info = {};
    submit_info.cmd_list_count = cmd_lists.size();
    submit_info.cmd_lists = cmd_lists.data();
    RHISemaphore* wait_semaphores[] = { m_image_available_semaphores[m_current_frame].get() };
    submit_info.semaphores_to_wait = wait_semaphores;
    submit_info.wait_semaphore_count = 1;
    submit_info.semaphore_to_signal = m_render_finished_semaphores[m_current_frame].get();
    RHIPipelineStageFlags stages_to_wait[] = { RHIPipelineStageFlags::ColorAttachmentOutput };
    submit_info.stages_to_wait = stages_to_wait;

    m_inflight_fences[m_current_frame] = m_rhi->SubmitCommandLists( submit_info );

    RHI::PresentInfo present_info = {};
    present_info.semaphore_count = 1;
    RHISemaphore* wait_semaphore = m_render_finished_semaphores[m_current_frame].get();
    present_info.wait_semaphores = &wait_semaphore;

    m_rhi->Present( *m_swapchain, present_info );

    m_current_frame = ( m_current_frame + 1 ) % m_max_frames_in_flight;
}

void EngineApp::InitCoreGlobals()
{
    g_core_paths.engine_content = m_cmd_line_args.engine_content_path;

    Logger::CreateInfo create_info = {};
    create_info.mirror_to_stdout = true;
    create_info.path = m_cmd_line_args.log_path.c_str();
    g_log = new Logger( create_info );
}

void EngineApp::InitEngineGlobals()
{
    SE_ENSURE( m_rhi && m_asset_mgr );

    g_engine.rhi = m_rhi.get();
    g_engine.asset_mgr = m_asset_mgr.get();
}

void EngineApp::CreateSyncObjects()
{
    m_image_available_semaphores.resize( m_max_frames_in_flight, VK_NULL_HANDLE );
    m_render_finished_semaphores.resize( m_max_frames_in_flight, VK_NULL_HANDLE );
    m_inflight_fences.resize( m_max_frames_in_flight, {} );
    m_imgui_frames.resize( m_max_frames_in_flight, {} );

    for ( size_t i = 0; i < m_max_frames_in_flight; ++i )
    {
        m_image_available_semaphores[i] = m_rhi->CreateGPUSemaphore();
        m_render_finished_semaphores[i] = m_rhi->CreateGPUSemaphore();
    }
}
