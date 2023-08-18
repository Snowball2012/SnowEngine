#include "StdAfx.h"

#include "SandboxApp.h"

#include <Engine/Assets.h>
#include <Engine/RHIUtils.h>
#include <Engine/Rendergraph.h>

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

CorePaths g_core_paths;
Logger* g_log;
EngineGlobals g_engine;

SandboxApp::SandboxApp()
    : EngineApp()
{
}

SandboxApp::~SandboxApp() = default;

void SandboxApp::ParseCommandLineDerived( int argc, char** argv )
{
    for ( int i = 1; i < argc; ++i )
    {
        if ( !_strcmpi( argv[i], "-DemoAssetPath" ) )
        {
            if ( argc > ( i + 1 ) )
            {
                // read next arg
                m_demo_mesh_asset_path = argv[i + 1];
                i += 1;


                SE_LOG_INFO( Sandbox, "Set demo mesh path from command line: %s", m_demo_mesh_asset_path );
            }
            else
            {
                SE_LOG_ERROR( Sandbox, "Can't set demo asset path from command line because -DemoAssetPath is the last token" );
            }
        }
    }
}

void SandboxApp::OnInit()
{
    SE_LOG_INFO( Sandbox, "Sandbox initialization started" );

    m_demo_mesh = boost::dynamic_pointer_cast< MeshAsset >( m_asset_mgr->Load( AssetId( m_demo_mesh_asset_path.c_str() ) ) );
    if ( !( m_demo_mesh && m_demo_mesh->GetStatus() == AssetStatus::Ready ) )
    {
        SE_LOG_FATAL_ERROR( Sandbox, "Could not load demo asset an path %s", m_demo_mesh_asset_path.c_str() );
    }

    m_scene = std::make_unique<Scene>();
    m_scene_view = std::make_unique<SceneView>( m_scene.get() );

    m_scene_view->SetExtents( m_swapchain->GetExtent() );
    
    CreateDescriptorSetLayout();
    CreateFullscreenQuadPipeline();

    CreateTextureSampler();
    CreateDescriptorSets();

    m_world = std::make_unique<World>();

    m_demo_mesh_entity = m_world->CreateEntity();
    m_world->AddComponent<NameComponent>( m_demo_mesh_entity ).name = "DemoMesh";
    m_world->AddComponent<TransformComponent>( m_demo_mesh_entity );
    auto& demo_mesh_component = m_world->AddComponent<MeshInstanceComponent>( m_demo_mesh_entity );
    demo_mesh_component.scene_mesh_instance = m_scene->AddMeshInstanceFromAsset( m_demo_mesh );

    SE_LOG_INFO( Sandbox, "Sandbox initialization complete" );
}

void SandboxApp::OnCleanup()
{
    SE_LOG_INFO( Sandbox, "Sandbox shutdown started" );

    m_world = nullptr;

    m_fsquad_descsets.clear();

    m_point_sampler = nullptr;

    CleanupPipeline();

    m_scene_view = nullptr;
    m_scene = nullptr;

    m_demo_mesh = nullptr;

    SE_LOG_INFO( Sandbox, "Sandbox shutdown complete" );
}

void SandboxApp::CleanupPipeline()
{
    m_draw_fullscreen_quad_pipeline = nullptr;
    m_shader_bindings_layout_fsquad = nullptr;
    m_dsl_fsquad = nullptr;
}

void SandboxApp::CreateDescriptorSetLayout()
{
    RHI::DescriptorViewRange ranges[2] = {};

    ranges[0].type = RHIShaderBindingType::TextureRO;
    ranges[0].count = 1;
    ranges[0].stages = RHIShaderStageFlags::PixelShader;

    ranges[1].type = RHIShaderBindingType::Sampler;
    ranges[1].count = 1;
    ranges[1].stages = RHIShaderStageFlags::PixelShader;

    RHI::DescriptorSetLayoutInfo binding_table = {};
    binding_table.ranges = ranges;
    binding_table.range_count = std::size( ranges );

    m_dsl_fsquad = m_rhi->CreateDescriptorSetLayout( binding_table );

    RHI::ShaderBindingLayoutInfo layout_info = {};
    RHIDescriptorSetLayout* dsls[1] = { m_dsl_fsquad.get() };
    layout_info.tables = dsls;
    layout_info.table_count = std::size( dsls );

    m_shader_bindings_layout_fsquad = m_rhi->CreateShaderBindingLayout( layout_info );
}

void SandboxApp::CreateDescriptorSets()
{
    m_fsquad_descsets.resize( m_max_frames_in_flight );
    for ( size_t i = 0; i < m_max_frames_in_flight; ++i )
    {
        m_fsquad_descsets[i] = m_rhi->CreateDescriptorSet( *m_dsl_fsquad );
    }
}

void SandboxApp::CreateTextureSampler()
{
    RHI::SamplerInfo info_point = {};
    m_point_sampler = m_rhi->CreateSampler( info_point );
}

void SandboxApp::CreateFullscreenQuadPipeline()
{
    RHI::ShaderCreateInfo create_info = {};
    std::wstring shader_path = ToWString( ToOSPath( "#engine/shaders/FullscreenQuad.hlsl" ).c_str() );
    create_info.filename = shader_path.c_str();

    create_info.frequency = RHI::ShaderFrequency::Vertex;
    create_info.entry_point = "FullScreenQuadVS";
    RHIObjectPtr<RHIShader> triangle_shader_vs = m_rhi->CreateShader( create_info );

    create_info.frequency = RHI::ShaderFrequency::Pixel;
    create_info.entry_point = "FullScreenQuadPS";
    RHIObjectPtr<RHIShader> triangle_shader_ps = m_rhi->CreateShader( create_info );

    RHIInputAssemblerInfo ia_info = {};

    RHIGraphicsPipelineInfo rhi_pipeline_info = {};

    rhi_pipeline_info.input_assembler = &ia_info;
    rhi_pipeline_info.vs = triangle_shader_vs.get();
    rhi_pipeline_info.ps = triangle_shader_ps.get();

    RHIPipelineRTInfo rt_info = {};
    rt_info.format = m_swapchain->GetFormat();
    rhi_pipeline_info.rts_count = 1;
    rhi_pipeline_info.rt_info = &rt_info;
    rhi_pipeline_info.binding_layout = m_shader_bindings_layout_fsquad.get();

    rhi_pipeline_info.rasterizer.cull_mode = RHICullModeFlags::None;

    m_draw_fullscreen_quad_pipeline = m_rhi->CreatePSO( rhi_pipeline_info );
}

void SandboxApp::OnDrawFrame( Rendergraph& framegraph, RHICommandList* ui_cmd_list )
{
    m_scene->Synchronize();

    class SandboxRenderExtension : public ISceneRenderExtension
    {
        SandboxApp& m_app;

        RGExternalTexture* m_rg_swapchain = nullptr;

        RGPass* m_blit_to_swapchain_pass = nullptr;
        RGPass* m_ui_pass = nullptr;

        RHICommandList* m_ui_cmd_list = nullptr;

    public:
        SandboxRenderExtension( SandboxApp& app, RHICommandList* ui_cmd_list ) : m_app( app ), m_ui_cmd_list( ui_cmd_list ) {}

        virtual void PostSetupRendergraph( Rendergraph& rg, RGTexture& sceneOutput ) override
        {
            RGExternalTextureDesc swapchain_desc = {};
            swapchain_desc.name = "swapchain";
            swapchain_desc.rhi_texture = m_app.m_swapchain->GetTexture();
            swapchain_desc.initial_layout = RHITextureLayout::Present;
            swapchain_desc.final_layout = RHITextureLayout::Present;

            m_rg_swapchain = rg.RegisterExternalTexture( swapchain_desc );

            m_blit_to_swapchain_pass = rg.AddPass( RHI::QueueType::Graphics, "BlitToSwapchain" );
            m_blit_to_swapchain_pass->UseTexture( *m_rg_swapchain, RGTextureUsage::RenderTarget );
            m_blit_to_swapchain_pass->UseTexture( sceneOutput, RGTextureUsage::ShaderRead );

            m_ui_pass = rg.AddPass( RHI::QueueType::Graphics, "DrawUI" );
            m_ui_pass->UseTexture( *m_rg_swapchain, RGTextureUsage::RenderTarget );
        }

        virtual void PostCompileRendergraph( Rendergraph& rg, RGTexture& sceneOutput ) override
        {
            RHICommandList* cmd_list = GetRHI().GetCommandList( RHI::QueueType::Graphics );

            cmd_list->Begin();

            m_blit_to_swapchain_pass->AddCommandList( *cmd_list );
            {
                m_app.m_fsquad_descsets[m_app.GetCurrentFrameIdx()]->BindTextureROView( 0, 0, *m_app.m_scene_view->GetFrameColorTextureROView() );
                m_app.m_fsquad_descsets[m_app.GetCurrentFrameIdx()]->BindSampler( 1, 0, *m_app.m_point_sampler );

                RHIPassRTVInfo rt = {};
                rt.load_op = RHILoadOp::Clear;
                rt.store_op = RHIStoreOp::Store;
                rt.rtv = m_app.m_swapchain->GetRTV();
                rt.clear_value.float32[3] = 1.0f;

                RHIPassInfo pass_info = {};
                pass_info.render_area = RHIRect2D{ .offset = glm::ivec2( 0,0 ), .extent = m_app.m_swapchain->GetExtent() };
                pass_info.render_targets = &rt;
                pass_info.render_targets_count = 1;
                cmd_list->BeginPass( pass_info );

                RHIViewport viewport = {};
                viewport.x = 0;
                viewport.y = 0;
                glm::uvec2 swapchain_extent = m_app.m_swapchain->GetExtent();
                viewport.width = float( swapchain_extent.x );
                viewport.height = float( swapchain_extent.y );
                viewport.min_depth = 0.0f;
                viewport.max_depth = 1.0f;

                RHIRect2D scissor = {};
                scissor.offset = { 0, 0 };
                scissor.extent = swapchain_extent;

                cmd_list->SetViewports( 0, &viewport, 1 );
                cmd_list->SetScissors( 0, &scissor, 1 );

                cmd_list->SetPSO( *m_app.m_draw_fullscreen_quad_pipeline );

                cmd_list->BindDescriptorSet( 0, *m_app.m_fsquad_descsets[m_app.GetCurrentFrameIdx()] );

                cmd_list->Draw( 3, 1, 0, 0 );

                cmd_list->EndPass();
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

    RenderSceneParams parms = {};
    parms.rg = &framegraph;
    parms.view = m_scene_view.get();
    parms.extension = &extension;

    m_renderer->RenderScene( parms );
}

void SandboxApp::OnUpdate()
{
    UpdateGui();

    static auto start_time = std::chrono::high_resolution_clock::now();

    auto current_time = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>( current_time - start_time ).count();

    auto* demo_tf = m_world->GetComponent<TransformComponent>( m_demo_mesh_entity );
    if ( !SE_ENSURE( demo_tf ) )
        return;

    demo_tf->tf.orientation = glm::angleAxis( time * glm::radians( 90.0f ), glm::vec3( 0, 1, 0 ) );

    m_scene_view->SetLookAt( glm::vec3( 2, 2, 2 ), glm::vec3( 0, 0, 0 ) );
    m_scene_view->SetFOV( glm::radians( m_fov_degrees ) );

    UpdateScene();
}

void SandboxApp::UpdateScene()
{
    // this should be somewhere in the engine code

    // @todo - run only for dirty transforms
    for ( const auto& [entity_id, tf, mesh_instance_component] : m_world->CreateView<TransformComponent, MeshInstanceComponent>() )
    {
        SceneMeshInstance* smi = m_scene->GetMeshInstance( mesh_instance_component.scene_mesh_instance );
        if ( !SE_ENSURE( smi ) )
            continue;

        smi->m_tf = tf.tf;
    }
}

void SandboxApp::OnSwapChainRecreated()
{
    m_scene_view->SetExtents( m_swapchain->GetExtent() );
}

void SandboxApp::UpdateGui()
{
    if ( ImGui::BeginMainMenuBar() )
    {
        if ( ImGui::MenuItem( "WorldOutliner" ) )
            m_show_world_outliner = true;

        if ( ImGui::MenuItem( "ImGUIDemo" ) )
            m_show_imgui_demo = true;

        if ( ImGui::MenuItem( "ReloadShaders" ) )
            m_rhi->ReloadAllShaders();

        ImGui::EndMainMenuBar();
    }

    if ( m_show_imgui_demo )
        ImGui::ShowDemoWindow( &m_show_imgui_demo );

    if ( m_show_world_outliner )
    {
        ImGui::Begin( "World", &m_show_world_outliner, ImGuiWindowFlags_None );
        {
            ImGui::Text( "Total: %llu entities", m_world->GetEntityCount() );
            bool create_entity = ImGui::Button( "Add enitity" );
            ImGui::SameLine();
            ImGui::InputText( "Name", &m_new_entity_name );

            if ( create_entity )
            {
                auto new_entity = m_world->CreateEntity();
                auto& name_component = m_world->AddComponent<NameComponent>( new_entity );
                name_component.name = std::move( m_new_entity_name );
            }

            for ( auto&& [id, name_comp] : m_world->CreateView<NameComponent>() )
            {
                ImGui::Text( name_comp.name.c_str() );
            }
        }
        ImGui::End();
    }

    ImGui::Begin( "Demo" );
    {
        ImGui::SliderFloat( "FoV (degrees)", &m_fov_degrees, 1.0f, 179.0f, "%.1f" );
    }
    ImGui::End();
}
