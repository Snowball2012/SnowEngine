#include "StdAfx.h"

#include "EngineApp.h"

#include "AssetManager.h"
#include "AutomationBridge.h"
#include "LevelObjects.h"
#include "Rendergraph.h"
#include "Scene.h"

#include <VulkanRHI/VulkanRHI.h>
#include <D3D12RHI/D3D12RHI.h>

#include <ImguiBackend/ImguiBackend.h>

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include <vulkan/vulkan.h>

CVAR_DEFINE( EnginePath, std::string, ".", "SnowEngine root content path" );
CVAR_DEFINE( LogPath, std::string, "Engine.log", "Engine log file path" );

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

    // Console must be the first system to be initialized. We can initialize cvars with command line arguments so all cvars must be have correct values at soon as they are added to the app
    InitConsole();

    ParseCommandLine( argc, argv );

    InitCoreGlobals();

    SDL_Init( SDL_INIT_EVERYTHING );
    m_main_wnd = SDL_CreateWindow( GetMainWindowName(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, m_window_size.x, m_window_size.y, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    InitRHI();
    InitImGUI();

    m_asset_mgr = std::make_unique<AssetManager>();

    LevelObject::RegisterTraits();

    InitEngineGlobals();

    m_renderer = std::make_unique<Renderer>();
    g_engine.renderer = m_renderer.get();

    SE_ENSURE( m_renderer->LoadDefaultAssets() );

    InitReadbacks();

    OnInit();

    std::filesystem::path content_path( g_core_paths.engine_content );
    if ( content_path.filename().empty() )
        content_path = content_path.parent_path();
    const std::filesystem::path engine_root = content_path.parent_path();
    m_automation_bridge = std::make_unique<AutomationBridge>( *m_console, engine_root );

    // Startup commands may rely on the world, renderer, or derived application.
    // Execute them only after every initialization hook has completed.
    for ( const std::string& command : m_startup_commands )
    {
        m_console->ProcessCommand( command.c_str() );
    }
    m_startup_commands.clear();

    MainLoop();

    OnCleanup();

    Cleanup();

    SDL_DestroyWindow( m_main_wnd );
    m_main_wnd = nullptr;

    SDL_Quit();
}

void EngineApp::ParseCommandLine( int argc, char** argv )
{
    const auto is_registered_argument = [this]( const char* argument )
    {
        if ( argument == nullptr || ( argument[0] != '+' && argument[0] != '-' ) || argument[1] == '\0' )
            return false;
        return m_console->HasCommand( argument + 1 ) || m_console->HasCVar( argument + 1 );
    };

    for ( int i = 1; i < argc; ++i )
    {
        if ( argv[i][0] != '+' && argv[i][0] != '-' )
            continue;

        const char* name = argv[i] + 1;
        if ( m_console->HasCommand( name ) )
        {
            std::string command = name;
            while ( argc > i + 1 && !is_registered_argument( argv[i + 1] ) )
            {
                command += " \"";
                command += argv[++i];
                command += "\"";
            }
            m_startup_commands.emplace_back( std::move( command ) );
            std::cout << "[App]: Queued startup command: " << m_startup_commands.back() << std::endl;
        }
        else if ( m_console->HasCVar( name ) )
        {
            if ( argc > i + 1 && !is_registered_argument( argv[i + 1] ) )
            {
                m_console->AddCVarInitialValueOverride( name, argv[++i] );
                std::cout << "[App]: Set startup CVar: " << name << " = " << argv[i] << std::endl;
            }
            else
            {
                std::cerr << "[Error][App]: Missing value for startup CVar: " << argv[i] << std::endl;
            }
        }
        else
        {
            std::cerr << "[Error][App]: Unknown startup command or CVar: " << argv[i] << std::endl;
        }
    }
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

    m_rendergraphs.resize( m_max_frames_in_flight );
    for ( auto& rg : m_rendergraphs )
    {
        rg = std::make_unique<Rendergraph>();
    }

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

            if ( event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_BACKQUOTE && event.key.repeat == 0 )
            {
                m_console_open = !m_console_open;
            }

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
        if ( m_automation_bridge )
            m_automation_bridge->Tick();
        m_console->ExecuteQueuedCommands();
        Update();
    }
    m_rhi->WaitIdle();
}

void EngineApp::Cleanup()
{
    SE_LOG_INFO( Engine, "RHI shutdown started" );

    m_automation_bridge.reset();

    m_readback_data.clear();

    m_imgui = nullptr;

    m_swapchain = nullptr;

    m_image_available_semaphores.clear();
    m_render_finished_semaphores.clear();
    m_inflight_fences.clear();

    m_renderer = nullptr;

    g_engine.asset_mgr = nullptr;
    m_asset_mgr.reset();

    m_rendergraphs.clear();

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
    m_renderer->DebugUI();
    DrawConsoleUI();
    OnUpdate();
    DrawFrame();
}

void EngineApp::DrawConsoleUI()
{
    if ( !m_console_open )
        return;

    ImGui::SetNextWindowSize( ImVec2( 720.0f, 420.0f ), ImGuiCond_FirstUseEver );
    if ( !ImGui::Begin( "SnowEngine Console", &m_console_open ) )
    {
        ImGui::End();
        return;
    }

    if ( ImGui::Button( "Clear" ) )
        m_console->ClearOutput();
    ImGui::SameLine();
    ImGui::TextUnformatted( "Enter a command, or a CVar name and optional value." );

    const float footer_height = ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild( "ConsoleOutput", ImVec2( 0, -footer_height ), true, ImGuiWindowFlags_HorizontalScrollbar );
    m_console->ReadOutput( []( std::span<const std::string> output )
    {
        for ( const std::string& line : output )
            ImGui::TextUnformatted( line.c_str() );
    } );
    if ( m_console_scroll_to_bottom )
    {
        ImGui::SetScrollHereY( 1.0f );
        m_console_scroll_to_bottom = false;
    }
    ImGui::EndChild();

    ImGui::SetNextItemWidth( -1.0f );
    constexpr ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_EnterReturnsTrue
        | ImGuiInputTextFlags_CallbackCompletion
        | ImGuiInputTextFlags_CallbackHistory
        | ImGuiInputTextFlags_CallbackEdit;
    const bool submitted = ImGui::InputText( "##ConsoleInput", &m_console_input, input_flags, &EngineApp::ConsoleInputCallback, this );
    const ImVec2 input_min = ImGui::GetItemRectMin();
    const ImVec2 input_size = ImGui::GetItemRectSize();
    const bool input_active = ImGui::IsItemActive();
    if ( ImGui::IsWindowAppearing() )
        ImGui::SetKeyboardFocusHere( -1 );
    if ( submitted )
    {
        if ( !m_console_input.empty() )
            m_console->QueueCommand( m_console_input );
        m_console_input.clear();
        m_console_autocomplete_options.clear();
        m_console_autocomplete_selection = 0;
        m_console_scroll_to_bottom = true;
        ImGui::SetKeyboardFocusHere( -1 );
    }

    if ( input_active && !m_console_autocomplete_options.empty() )
    {
        constexpr float autocomplete_alpha = 0.86f;
        const float autocomplete_max_height = ImGui::GetTextLineHeightWithSpacing() * 10.0f
            + ImGui::GetStyle().WindowPadding.y * 2.0f;
        ImGui::SetNextWindowPos( input_min, ImGuiCond_Always, ImVec2( 0.0f, 1.0f ) );
        ImGui::SetNextWindowBgAlpha( autocomplete_alpha );
        ImGui::SetNextWindowSizeConstraints(
            ImVec2( input_size.x, 0.0f ),
            ImVec2( input_size.x, autocomplete_max_height ) );
        constexpr ImGuiWindowFlags autocomplete_flags = ImGuiWindowFlags_AlwaysAutoResize
            | ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_NoFocusOnAppearing
            | ImGuiWindowFlags_NoInputs
            | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_Tooltip;

        if ( ImGui::Begin( "##ConsoleAutocomplete", nullptr, autocomplete_flags ) )
        {
            for ( size_t i = 0; i < m_console_autocomplete_options.size(); ++i )
            {
                const bool selected = i == m_console_autocomplete_selection;
                ImGui::Selectable( m_console_autocomplete_options[i].c_str(), selected );
                if ( selected && m_console_autocomplete_scroll_to_selection )
                    ImGui::SetScrollHereY( 0.5f );
            }
        }
        ImGui::End();
        m_console_autocomplete_scroll_to_selection = false;
    }

    ImGui::End();
}

void EngineApp::RefreshConsoleAutocomplete( std::string_view input )
{
    const std::string selected_option = m_console_autocomplete_options.empty()
        ? std::string()
        : m_console_autocomplete_options[m_console_autocomplete_selection];

    const size_t first_space = input.find_first_of( " \t\r\n" );
    if ( first_space != std::string_view::npos )
    {
        m_console_autocomplete_options.clear();
        m_console_autocomplete_selection = 0;
        return;
    }

    m_console->GetAutocompleteOptions( input, m_console_autocomplete_options );
    m_console_autocomplete_selection = 0;
    if ( !selected_option.empty() )
    {
        const auto selected_it = std::find(
            m_console_autocomplete_options.begin(),
            m_console_autocomplete_options.end(),
            selected_option );
        if ( selected_it != m_console_autocomplete_options.end() )
            m_console_autocomplete_selection = static_cast<size_t>( selected_it - m_console_autocomplete_options.begin() );
    }
}

int EngineApp::HandleConsoleInput( ImGuiInputTextCallbackData* data )
{
    if ( data->EventFlag == ImGuiInputTextFlags_CallbackEdit )
    {
        RefreshConsoleAutocomplete( std::string_view( data->Buf, data->BufTextLen ) );
    }
    else if ( data->EventFlag == ImGuiInputTextFlags_CallbackHistory && !m_console_autocomplete_options.empty() )
    {
        if ( data->EventKey == ImGuiKey_UpArrow )
        {
            if ( m_console_autocomplete_selection == 0 )
                m_console_autocomplete_selection = m_console_autocomplete_options.size() - 1;
            else
                --m_console_autocomplete_selection;
        }
        else if ( data->EventKey == ImGuiKey_DownArrow )
        {
            m_console_autocomplete_selection = ( m_console_autocomplete_selection + 1 )
                % m_console_autocomplete_options.size();
        }
        m_console_autocomplete_scroll_to_selection = true;
    }
    else if ( data->EventFlag == ImGuiInputTextFlags_CallbackCompletion )
    {
        RefreshConsoleAutocomplete( std::string_view( data->Buf, data->BufTextLen ) );
        if ( !m_console_autocomplete_options.empty() )
        {
            const std::string completion = m_console_autocomplete_options[m_console_autocomplete_selection];
            data->DeleteChars( 0, data->BufTextLen );
            data->InsertChars( 0, completion.c_str() );
            data->InsertChars( data->CursorPos, " " );
            m_console_autocomplete_options.clear();
            m_console_autocomplete_selection = 0;
        }
    }
    return 0;
}

int EngineApp::ConsoleInputCallback( ImGuiInputTextCallbackData* data )
{
    return static_cast<EngineApp*>( data->UserData )->HandleConsoleInput( data );
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

    if ( m_readback_valid )
    {
        OnFrameRenderFinish( m_readback_data[m_current_frame] );
    }

    if ( m_fb_resized )
    {
        m_swapchain->Recreate();
        m_fb_resized = false;
        OnSwapChainRecreated();
    }
    bool swapchain_recreated = false;
    m_swapchain->AcquireNextImage( m_image_available_semaphores[m_current_frame].get(), swapchain_recreated );

    ImguiRenderResult imgui_res = m_imgui->RenderFrame( *m_swapchain->GetRTV() );
    m_imgui_frames[m_current_frame] = imgui_res.frame_idx;

    Rendergraph& framegraph = *m_rendergraphs[m_current_frame].get();
    framegraph.Reset();

    OnDrawFrame( framegraph, m_readback_data[m_current_frame], imgui_res.cl );

    RGSubmitInfo fg_submit_info = {};
    RHISemaphore* wait_semaphores[] = { m_image_available_semaphores[m_current_frame].get() };
    fg_submit_info.semaphores_to_wait = wait_semaphores;
    fg_submit_info.wait_semaphore_count = 1;
    fg_submit_info.semaphore_to_signal = m_render_finished_semaphores[m_current_frame].get();
    RHIPipelineStageFlags stages_to_wait[] = { RHIPipelineStageFlags::ColorAttachmentOutput };
    fg_submit_info.stages_to_wait = stages_to_wait;

    m_inflight_fences[m_current_frame] = framegraph.Submit( fg_submit_info );

    RHI::PresentInfo present_info = {};
    present_info.semaphore_count = 1;
    RHISemaphore* wait_semaphore = m_render_finished_semaphores[m_current_frame].get();
    present_info.wait_semaphores = &wait_semaphore;

    m_rhi->Present( *m_swapchain, present_info );

    m_current_frame = ( m_current_frame + 1 ) % m_max_frames_in_flight;
    if ( m_current_frame == 0 )
    {
        // that means we saturated frame queue and can start receiving initialized readback structure
        m_readback_valid = true;
    }
}

void EngineApp::InitCoreGlobals()
{
    std::filesystem::path engine_content = std::filesystem::path( EnginePath.GetValue() ) / "EngineContent";
    g_core_paths.engine_content = engine_content.generic_string();
    if ( !g_core_paths.engine_content.ends_with( '/' ) )
        g_core_paths.engine_content += '/';

    Logger::CreateInfo create_info = {};
    create_info.mirror_to_stdout = true;
    create_info.path = LogPath.GetValue().c_str();
    g_log = new Logger( create_info );
    g_log->SetCallback( &Console::LogCallback, m_console.get() );
}

void EngineApp::InitConsole()
{
    m_console = std::make_unique<Console>();
    m_console->AddCVars( g_cvars_head );
    // RHI CVars must be registered before command-line parsing so their
    // startup overrides are applied before RHI initialization.
    m_console->AddCVars( VulkanRHI_GetCVarListHead() );
    m_console->AddCommands( g_console_commands_head );
}

void EngineApp::InitEngineGlobals()
{
    SE_ENSURE( m_rhi && m_asset_mgr && m_console );

    g_engine.rhi = m_rhi.get();
    g_engine.asset_mgr = m_asset_mgr.get();
    g_engine.console = m_console.get();
}

void EngineApp::InitReadbacks()
{
    m_readback_data.resize( m_max_frames_in_flight );
    for ( auto& data : m_readback_data )
    {
        CreateReadbackData( data );
    }
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
