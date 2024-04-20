#include "StdAfx.h"

#include "SandboxApp.h"

#include <Engine/Rendergraph.h>
#include <Engine/ShaderPrograms.h>

CorePaths g_core_paths;
Logger* g_log;
EngineGlobals g_engine;
ConsoleVariableBase* g_cvars_head = nullptr;

SandboxApp::SandboxApp()
    : EngineApp()
{
    m_last_tick_time = std::chrono::high_resolution_clock::now();
}

SandboxApp::~SandboxApp() = default;

void SandboxApp::ParseCommandLineDerived( int argc, char** argv )
{
    for ( int i = 1; i < argc; ++i )
    {
        if ( !_strcmpi( argv[i], "-StartLevel" ) )
        {
            if ( argc > ( i + 1 ) )
            {
                // read next arg
                m_current_level_path = argv[i + 1];
                i += 1;


                SE_LOG_INFO( Sandbox, "Set level path from command line: %s", m_current_level_path );
            }
            else
            {
                SE_LOG_ERROR( Sandbox, "Can't set level path from command line because -StartLevel is the last token" );
            }
        }
    }
}

void SandboxApp::OnInit()
{
    SE_LOG_INFO( Sandbox, "Sandbox initialization started" );

    m_editor = std::make_unique<LevelEditor>();
    m_editor->SetViewportExtents( m_swapchain->GetExtent() );

    if ( !m_editor->OpenLevel( ToOSPath( m_current_level_path.c_str() ).c_str() ) )
    {
        SE_LOG_ERROR( Sandbox, "Could not open level at path %s", m_current_level_path.c_str() );
    }

    SE_LOG_INFO( Sandbox, "Sandbox initialization complete" );
}

void SandboxApp::OnCleanup()
{
    SE_LOG_INFO( Sandbox, "Sandbox shutdown started" );

    m_editor = nullptr;

    SE_LOG_INFO( Sandbox, "Sandbox shutdown complete" );
}

void SandboxApp::OnDrawFrame( Rendergraph& framegraph, const AppGPUReadbackData& readback_data, RHICommandList* ui_cmd_list )
{
    class SandboxRenderExtension : public ISceneRenderExtension
    {
        SandboxApp& m_app;

        RGExternalTexture* m_rg_swapchain = nullptr;

        RGPass* m_blit_to_swapchain_pass = nullptr;
        RGPass* m_ui_pass = nullptr;

        RHICommandList* m_ui_cmd_list = nullptr;

    public:
        SandboxRenderExtension( SandboxApp& app, RHICommandList* ui_cmd_list ) : m_app( app ), m_ui_cmd_list( ui_cmd_list ) {}

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

            m_ui_pass = data.rg->AddPass( RHI::QueueType::Graphics, "DrawUI" );
            m_ui_pass->UseTextureView( *swapchain_rt_view );
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

            // Dirty hack. ui_pass may want to add extra commands at the start of it's first command list, but ui_cmd_list is already filled. Borrow list from last pass to insert those commands
            m_ui_pass->BorrowCommandList( *cmd_list );
            cmd_list->End();

            if ( m_ui_cmd_list != nullptr )
            {
                m_ui_pass->AddCommandList( *m_ui_cmd_list );
            }
            m_ui_pass->EndPass();
        }

    } extension( *this, ui_cmd_list );

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
}

void SandboxApp::CreateReadbackData( AppGPUReadbackData& data )
{
    data.buffer = GetRenderer().CreateViewFrameReadbackBuffer();
}

void SandboxApp::UpdateGui()
{
    if ( ImGui::BeginMainMenuBar() )
    {
        if ( ImGui::BeginMenu( "Level" ) )
        {
            if ( ImGui::MenuItem( "New" ) )
            {
                m_editor->OpenLevel( ToOSPath( "#engine/Levels/Default.sel" ).c_str() );
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
                m_show_material_editor = true;

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

    if ( m_show_material_editor )
    {
        ImGui::Begin( "Material Editor", &m_show_material_editor, ImGuiWindowFlags_None );
        ImGui::End();
    }
}
