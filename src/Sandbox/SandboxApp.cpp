#include "StdAfx.h"

#include "SandboxApp.h"

#include <Engine/Rendergraph.h>
#include <Engine/ShaderPrograms.h>

CorePaths g_core_paths;
Logger* g_log;
EngineGlobals g_engine;
ConsoleVariableBase* g_cvars_head = nullptr;

CVAR_DEFINE( StartLevel, std::string, "#engine/Levels/Default.sel", "Level loaded by Sandbox at startup" );

namespace
{
    SandboxApp* g_sandbox_app = nullptr;
}

CONSOLE_COMMAND_DEFINE( screenshot, "Capture the swapchain: screenshot [with_imgui|without_imgui] [output.png]" )
{
    bool include_imgui = false;
    std::string_view output_path;

    if ( args.size() > 2 )
    {
        output = "Usage: screenshot [with_imgui|without_imgui] [output.png]";
        return false;
    }

    if ( !args.empty() )
    {
        if ( !_stricmp( args[0].c_str(), "with_imgui" ) )
        {
            include_imgui = true;
        }
        else if ( !_stricmp( args[0].c_str(), "without_imgui" ) )
        {
            include_imgui = false;
        }
        else if ( args.size() == 1 )
        {
            output_path = args[0];
        }
        else
        {
            output = "First argument must be with_imgui or without_imgui";
            return false;
        }
    }

    if ( args.size() == 2 )
        output_path = args[1];

    if ( !g_sandbox_app )
    {
        output = "Sandbox is not initialized";
        return false;
    }

    return g_sandbox_app->QueueScreenshot( include_imgui, output_path, output );
}

SandboxApp::SandboxApp()
    : EngineApp()
{
    VERIFY_EQUALS( g_sandbox_app, nullptr );
    g_sandbox_app = this;
    m_last_tick_time = std::chrono::high_resolution_clock::now();
}

SandboxApp::~SandboxApp()
{
    if ( g_sandbox_app == this )
        g_sandbox_app = nullptr;
}

bool SandboxApp::QueueScreenshot( bool include_imgui, std::string_view output_path, std::string& output )
{
    if ( m_screenshot_request.active )
    {
        output = "A screenshot is already queued";
        return false;
    }

    const RHIFormat format = m_swapchain->GetFormat();
    if ( format != RHIFormat::B8G8R8A8_SRGB &&
         format != RHIFormat::R8G8B8A8_SRGB &&
         format != RHIFormat::R8G8B8A8_UNORM )
    {
        output = "The swapchain format is not supported for PNG screenshots";
        return false;
    }

    std::filesystem::path path;
    if ( output_path.empty() )
    {
        const uint64_t timestamp = uint64_t( std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch() ).count() );
        path = std::filesystem::path( g_core_paths.engine_content ) / "Screenshots" /
            ( "Screenshot_" + std::to_string( timestamp ) + "_" + std::to_string( ++m_screenshot_sequence ) + ".png" );
    }
    else
    {
        path = std::filesystem::path( output_path );
        if ( path.is_relative() )
            path = std::filesystem::path( g_core_paths.engine_content ) / "Screenshots" / path;
        if ( path.extension().empty() )
            path += ".png";
    }

    std::error_code error;
    if ( std::filesystem::exists( path, error ) )
    {
        output = "Screenshot output already exists: " + path.generic_string();
        return false;
    }
    if ( error )
    {
        output = "Could not inspect screenshot output path: " + error.message();
        return false;
    }

    m_screenshot_request.active = true;
    m_screenshot_request.include_imgui = include_imgui;
    m_screenshot_request.output_path = path.generic_string();

    output = "Screenshot queued (" +
        std::string( include_imgui ? "with_imgui" : "without_imgui" ) +
        "): " + m_screenshot_request.output_path;
    return true;
}

void SandboxApp::OnInit()
{
    SE_LOG_INFO( Sandbox, "Sandbox initialization started" );

    m_editor = std::make_unique<LevelEditor>();
    m_material_editor = std::make_unique<MaterialEditor>();
    m_editor->SetEditorActions( m_material_editor.get() );
    m_editor->SetViewportExtents( m_swapchain->GetExtent() );

    const std::string& start_level = StartLevel.GetValue();
    if ( !m_editor->OpenLevel( start_level.c_str() ) )
    {
        SE_LOG_ERROR( Sandbox, "Could not open level at path %s", start_level.c_str() );
    }

    SE_LOG_INFO( Sandbox, "Sandbox initialization complete" );
}

void SandboxApp::OnCleanup()
{
    SE_LOG_INFO( Sandbox, "Sandbox shutdown started" );

    m_editor->SetEditorActions( nullptr );
    m_material_editor = nullptr;
    m_editor = nullptr;

    SE_LOG_INFO( Sandbox, "Sandbox shutdown complete" );
}

SandboxApp::ScreenshotReadback* SandboxApp::BeginScreenshotReadback()
{
    if ( !m_screenshot_request.active )
        return nullptr;

    ScreenshotReadback& readback = m_screenshot_readbacks[GetCurrentFrameIdx()];
    if ( !SE_ENSURE( !readback.active ) )
        return nullptr;

    const glm::uvec2 extent = m_swapchain->GetExtent();
    RHI::BufferInfo buffer_info = {};
    buffer_info.size = uint64_t( extent.x ) * uint64_t( extent.y ) * 4;
    buffer_info.usage = RHIBufferUsageFlags::TransferDst;
    buffer_info.name = "ScreenshotReadback";

    readback.buffer = GetRHI().CreateReadbackBuffer( buffer_info );
    if ( !readback.buffer )
    {
        SE_LOG_ERROR( Sandbox, "Could not allocate screenshot readback buffer" );
        m_screenshot_request = {};
        return nullptr;
    }

    readback.active = true;
    readback.include_imgui = m_screenshot_request.include_imgui;
    readback.extent = extent;
    readback.format = m_swapchain->GetFormat();
    readback.output_path = std::move( m_screenshot_request.output_path );
    m_screenshot_request = {};
    return &readback;
}

void SandboxApp::OnDrawFrame( Rendergraph& framegraph, const AppGPUReadbackData& readback_data, RHICommandList* ui_cmd_list )
{
    ScreenshotReadback* screenshot_readback = BeginScreenshotReadback();

    class SandboxRenderExtension : public ISceneRenderExtension
    {
        SandboxApp& m_app;
        ScreenshotReadback* m_screenshot_readback = nullptr;

        RGExternalTexture* m_rg_swapchain = nullptr;

        RGPass* m_blit_to_swapchain_pass = nullptr;
        RGPass* m_screenshot_pass = nullptr;
        RGPass* m_ui_pass = nullptr;

        RHICommandList* m_ui_cmd_list = nullptr;

    public:
        SandboxRenderExtension(
            SandboxApp& app,
            ScreenshotReadback* screenshot_readback,
            RHICommandList* ui_cmd_list )
            : m_app( app )
            , m_screenshot_readback( screenshot_readback )
            , m_ui_cmd_list( ui_cmd_list )
        {
        }

        virtual void PostSetupRendergraph( const SceneViewFrameData& data ) override
        {
            RGExternalTextureDesc swapchain_desc = {};
            swapchain_desc.name = "swapchain";
            swapchain_desc.rhi_texture = m_app.m_swapchain->GetTexture();
            swapchain_desc.initial_layout = RHITextureLayout::Present;
            swapchain_desc.final_layout = RHITextureLayout::Present;

            m_rg_swapchain = data.rg->RegisterExternalTexture( swapchain_desc );
            const RGRenderTargetView* swapchain_rt_view = m_rg_swapchain->RegisterExternalRTView( { m_app.m_swapchain->GetRTV() } );

            m_blit_to_swapchain_pass = data.rg->AddPass( RHI::QueueType::Graphics, "BlitToSwapchain" );
            m_blit_to_swapchain_pass->UseTextureView( *swapchain_rt_view );
            m_blit_to_swapchain_pass->UseTextureView( *data.scene_output[data.scene_output_idx]->GetROView() );

            if ( m_screenshot_readback && !m_screenshot_readback->include_imgui )
            {
                m_screenshot_pass = data.rg->AddPass( RHI::QueueType::Graphics, "CaptureScreenshotWithoutImGui" );
                m_screenshot_pass->UseTexture( *m_rg_swapchain, RGTextureUsage::TransferSrc );
            }

            m_ui_pass = data.rg->AddPass( RHI::QueueType::Graphics, "DrawUI" );
            m_ui_pass->UseTextureView( *swapchain_rt_view );

            if ( m_screenshot_readback && m_screenshot_readback->include_imgui )
            {
                m_screenshot_pass = data.rg->AddPass( RHI::QueueType::Graphics, "CaptureScreenshotWithImGui" );
                m_screenshot_pass->UseTexture( *m_rg_swapchain, RGTextureUsage::TransferSrc );
            }
        }

        virtual void PostCompileRendergraph( const SceneViewFrameData& data ) override
        {
            RHICommandList* cmd_list = GetRHI().GetCommandList( RHI::QueueType::Graphics );

            cmd_list->Begin();

            m_blit_to_swapchain_pass->AddCommandList( *cmd_list );
            {
                const BlitTextureProgram* blit_prog = m_app.m_renderer->GetBlitTextureProgram();

                BlitTextureProgram::Params parms = {};
                {
                    parms.output = m_rg_swapchain->GetRTView()->GetRHIView();
                    parms.input = data.scene_output[data.scene_output_idx]->GetROView()->GetRHIView();
                    parms.sampler = m_app.m_renderer->GetPointSampler();
                }

                blit_prog->Run( *cmd_list, *data.rg, parms );
            }
            m_blit_to_swapchain_pass->EndPass();

            if ( m_screenshot_readback && !m_screenshot_readback->include_imgui )
            {
                m_screenshot_pass->BorrowCommandList( *cmd_list );
                RecordScreenshotCopy( *cmd_list );
                m_screenshot_pass->EndPass();
            }

            // Dirty hack. ui_pass may want to add extra commands at the start of it's first command list, but ui_cmd_list is already filled. Borrow list from last pass to insert those commands
            m_ui_pass->BorrowCommandList( *cmd_list );
            cmd_list->End();

            if ( m_ui_cmd_list != nullptr )
            {
                m_ui_pass->AddCommandList( *m_ui_cmd_list );
            }
            m_ui_pass->EndPass();

            if ( m_screenshot_readback && m_screenshot_readback->include_imgui )
            {
                RHICommandList* screenshot_cmd_list = GetRHI().GetCommandList( RHI::QueueType::Graphics );
                screenshot_cmd_list->Begin();
                m_screenshot_pass->AddCommandList( *screenshot_cmd_list );
                RecordScreenshotCopy( *screenshot_cmd_list );
                screenshot_cmd_list->End();
                m_screenshot_pass->EndPass();
            }
        }

    private:
        void RecordScreenshotCopy( RHICommandList& cmd_list )
        {
            RHIBufferTextureCopyRegion region = {};
            region.texture_extent[0] = m_screenshot_readback->extent.x;
            region.texture_extent[1] = m_screenshot_readback->extent.y;
            region.texture_extent[2] = 1;
            region.texture_subresource.mip_count = 1;
            region.texture_subresource.array_count = 1;

            cmd_list.CopyTextureToBuffer(
                *m_rg_swapchain->GetRHITexture(),
                *m_screenshot_readback->buffer->GetBuffer(),
                &region,
                1 );
        }

    } extension( *this, screenshot_readback, ui_cmd_list );

    m_editor->Draw( framegraph, &extension, readback_data.buffer->GetBuffer() );
}

void SandboxApp::OnUpdate()
{
    UpdateGui();

    // @todo - some helper to manage app time
    static auto start_time = std::chrono::high_resolution_clock::now();

    auto current_time = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>( current_time - start_time ).count();
    float delta_time = std::chrono::duration<float, std::chrono::seconds::period>( current_time - m_last_tick_time ).count();
    m_last_tick_time = current_time;

    m_editor->Update( delta_time );
    if ( m_material_editor ) m_material_editor->OnUpdate( *m_editor );
}


void SandboxApp::OnSwapChainRecreated()
{
    m_editor->SetViewportExtents( m_swapchain->GetExtent() );
}

void SandboxApp::OnFrameRenderFinish( const AppGPUReadbackData& data )
{
    ViewFrameReadbackData readback_data = {};
    data.buffer->ReadBytes( &readback_data, sizeof( readback_data ) );

    // @todo: some kind of subscription for systems that want to use readback data?
    if ( m_editor )
    {
        m_editor->UpdateReadback( readback_data );
    }

    ScreenshotReadback& screenshot = m_screenshot_readbacks[GetCurrentFrameIdx()];
    if ( screenshot.active )
        CompleteScreenshotReadback( screenshot );
}

void SandboxApp::CreateReadbackData( AppGPUReadbackData& data )
{
    data.buffer = GetRenderer().CreateViewFrameReadbackBuffer();
}

void SandboxApp::CompleteScreenshotReadback( ScreenshotReadback& screenshot )
{
    const size_t pixel_count = size_t( screenshot.extent.x ) * size_t( screenshot.extent.y );
    std::vector<uint8_t> pixels( pixel_count * 4 );
    screenshot.buffer->ReadBytes( pixels.data(), pixels.size() );

    if ( screenshot.format == RHIFormat::B8G8R8A8_SRGB )
    {
        for ( size_t pixel_idx = 0; pixel_idx < pixel_count; ++pixel_idx )
            std::swap( pixels[pixel_idx * 4], pixels[pixel_idx * 4 + 2] );
    }

    const std::filesystem::path output_path( screenshot.output_path );
    std::filesystem::path temporary_path = output_path;
    temporary_path += ".tmp";

    std::error_code error;
    if ( !output_path.parent_path().empty() )
        std::filesystem::create_directories( output_path.parent_path(), error );

    bool success = !error;
    if ( success )
    {
        success = stbi_write_png(
            temporary_path.string().c_str(),
            int( screenshot.extent.x ),
            int( screenshot.extent.y ),
            4,
            pixels.data(),
            0 ) != 0;
    }

    if ( success )
    {
        std::filesystem::rename( temporary_path, output_path, error );
        success = !error;
    }

    if ( success )
    {
        SE_LOG_INFO(
            Sandbox,
            "Screenshot saved (%s): %s",
            screenshot.include_imgui ? "with_imgui" : "without_imgui",
            screenshot.output_path.c_str() );
    }
    else
    {
        SE_LOG_ERROR(
            Sandbox,
            "Could not save screenshot %s: %s",
            screenshot.output_path.c_str(),
            error ? error.message().c_str() : "PNG encoding failed" );

        std::error_code remove_error;
        std::filesystem::remove( temporary_path, remove_error );
    }

    screenshot = {};
}

void SandboxApp::UpdateGui()
{
    if ( ImGui::BeginMainMenuBar() )
    {
        if ( ImGui::BeginMenu( "Level" ) )
        {
            if ( ImGui::MenuItem( "New" ) )
            {
                m_editor->OpenLevel( "#engine/Levels/Default.sel" );
            }

            if ( ImGui::MenuItem( "Open" ) )
            {
                std::string default_path = g_core_paths.engine_content + "/Levels/";
                ImGuiFileDialog::Instance()->OpenDialog( "LevelOpenDlg", "Open Level", ".sel", default_path.c_str() );
            }

            if ( ImGui::MenuItem( "Save" ) )
            {
                std::string default_path = g_core_paths.engine_content + "/Levels/";
                ImGuiFileDialog::Instance()->OpenDialog( "LevelSaveDlg", "Save Level As", ".sel", default_path.c_str() );
            }

            ImGui::EndMenu();
        }

        if ( ImGui::BeginMenu( "Tools" ) ) {

            if ( ImGui::MenuItem( "Material Editor" ) )
            {
                if ( m_material_editor )
                    m_material_editor->Show();
            }

            if ( ImGui::MenuItem( "WorldOutliner" ) )
                m_show_world_outliner = true;

            if ( ImGui::MenuItem( "ImGUIDemo" ) )
                m_show_imgui_demo = true;

            ImGui::EndMenu();
        }

        if ( ImGui::MenuItem( "ReloadShaders" ) )
        {
            m_rhi->ReloadAllShaders();
            m_editor->ResetAccumulation();
        }

        if ( ImGui::MenuItem( "PrintCVars" ) )
            GetConsole().ListAllCVars();

        ImGui::EndMainMenuBar();
    }

    if ( ImGuiFileDialog::Instance()->Display( "LevelSaveDlg" ) )
    {
        if ( ImGuiFileDialog::Instance()->IsOk() )
        {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
            std::string fileName = ImGuiFileDialog::Instance()->GetCurrentFileName();

            m_editor->SaveLevel( filePathName.c_str() );
        }

        ImGuiFileDialog::Instance()->Close();
    }

    if ( ImGuiFileDialog::Instance()->Display( "LevelOpenDlg" ) )
    {
        if ( ImGuiFileDialog::Instance()->IsOk() )
        {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
            std::string fileName = ImGuiFileDialog::Instance()->GetCurrentFileName();

            m_editor->OpenLevel( filePathName.c_str() );
        }

        ImGuiFileDialog::Instance()->Close();
    }

    if ( m_show_imgui_demo )
        ImGui::ShowDemoWindow( &m_show_imgui_demo );

    if ( m_show_world_outliner )
    {
        ImGui::Begin( "World", &m_show_world_outliner, ImGuiWindowFlags_None );
        {
            World* current_world = m_editor->GetWorld();
            if ( SE_ENSURE( current_world != nullptr ) )
            {
                ImGui::Text( "Total: %llu entities", current_world->GetEntityCount() );

                for ( auto&& [id, name_comp] : current_world->CreateView<NameComponent>() )
                {
                    ImGui::Text( name_comp.name.c_str() );
                }
            }
            else
            {
                ImGui::Text( "World not found" );
            }
        }
        ImGui::End();
    }


}
