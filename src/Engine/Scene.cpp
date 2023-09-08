#include "StdAfx.h"

#include "Scene.h"

#include "DescriptorSetPool.h"
#include "DisplayMapping.h"
#include "ShaderPrograms.h"
#include "Rendergraph.h"
#include "UploadBufferPool.h"

#include <ImguiBackend/ImguiBackend.h>


CVAR_DEFINE( r_debugDisplayMapping, int, 0, "Show debug interface for display mapping" );

// Scene

Scene::Scene()
{
    m_tlas = std::make_unique<TLAS>();
}

SceneMeshInstanceID Scene::AddMeshInstanceFromAsset( MeshAssetPtr base_asset )
{
    if ( !SE_ENSURE( base_asset ) )
        return SceneMeshInstanceID::nullid;

    SceneMeshInstanceID new_mesh_id = m_mesh_instances.emplace();

    auto& mesh_instance = m_mesh_instances[new_mesh_id];

    mesh_instance.m_asset = base_asset;
    mesh_instance.m_tlas_instance = m_tlas->Instances().emplace();
    m_tlas->Instances()[mesh_instance.m_tlas_instance].blas = base_asset->GetAccelerationStructure();

    return new_mesh_id;
}

void Scene::RemoveMeshInstance( SceneMeshInstanceID id )
{
    SceneMeshInstance* instance = m_mesh_instances.try_get( id );
    if ( !instance )
        return;

    m_tlas->Instances().erase( instance->m_tlas_instance );

    m_mesh_instances.erase( id );
}

void Scene::Synchronize()
{
    UpdateTLASTransforms();
}

void Scene::UpdateTLASTransforms()
{
    for ( SceneMeshInstance& mesh_instance : m_mesh_instances )
    {
        m_tlas->Instances()[mesh_instance.m_tlas_instance].transform = ToMatrixRowMajor3x4( mesh_instance.m_tf );
    }
}


// SceneView

SceneView::SceneView( Scene* scene )
    : m_scene( scene )
{
    VERIFY_NOT_EQUAL( m_scene, nullptr );
}

void SceneView::SetLookAt( const glm::vec3& eye, const glm::vec3& center )
{
    m_eye_pos = eye;
    m_eye_orientation = glm::quatLookAt( glm::normalize( center - eye ), m_up );
}

void SceneView::SetExtents( const glm::uvec2& extents )
{
    m_extents = extents;

    RHI::TextureInfo tex_info = {};
    tex_info.dimensions = RHITextureDimensions::T2D;
    tex_info.width = m_extents.x;
    tex_info.height = m_extents.y;
    tex_info.depth = 1;
    tex_info.mips = 1;
    tex_info.array_layers = 1;
    tex_info.format = RHIFormat::R8G8B8A8_UNORM;
    tex_info.usage = RHITextureUsageFlags::TextureROView | RHITextureUsageFlags::TextureRWView | RHITextureUsageFlags::RenderTargetView;
    tex_info.initial_layout = RHITextureLayout::ShaderReadOnly;

    for ( int i = 0; i < 2; ++i )
    {
        m_rt_frame[i] = GetRHI().CreateTexture(tex_info);

        RHI::TextureRWViewInfo uav_info = {};
        uav_info.texture = m_rt_frame[i].get();
        m_frame_rwview[i] = GetRHI().CreateTextureRWView(uav_info);

        RHI::TextureROViewInfo srv_info = {};
        srv_info.texture = m_rt_frame[i].get();
        m_frame_roview[i] = GetRHI().CreateTextureROView(srv_info);

        RHI::RenderTargetViewInfo rtv_info = {};
        rtv_info.texture = m_rt_frame[i].get();
        rtv_info.format = tex_info.format;
        m_frame_rtview[i] = GetRHI().CreateRTV(rtv_info);
    }
}

glm::mat4x4 SceneView::CalcViewMatrix() const
{
    // @todo - i don't like inverses here
    return glm::translate( glm::toMat4( glm::inverse( m_eye_orientation ) ), -m_eye_pos );
}

glm::mat4x4 SceneView::CalcProjectionMatrix() const
{
    float aspectRatio = float( m_extents.x ) / float( m_extents.y );
    float fovY = m_fovXRadians / aspectRatio;
    return glm::perspective( fovY, aspectRatio, 0.1f, 10.0f );
}


// Renderer

Renderer::Renderer()
{
    if ( !GetRHI().SupportsRaytracing() )
    {
        SE_LOG_FATAL_ERROR( Renderer, "Raytracing is not supported on the GPU" );
    }

    CreateDescriptorSetLayout();
    CreatePrograms();
    CreateSamplers();
    CreateRTPipeline();

    m_display_mapping = std::make_unique<DisplayMapping>();
}

Renderer::~Renderer() = default;

namespace
{
    // must be in sync with SceneViewParams.hlsli
    struct GPUSceneViewParams
    {
        glm::mat4 view_mat;
        glm::mat4 proj_mat;
        glm::mat4 view_proj_inv_mat;
        glm::uvec2 viewport_size_px;
    };
}

void Renderer::CreateRTPipeline()
{
    RHI::ShaderCreateInfo create_info = {};
    std::wstring shader_path = ToWString( ToOSPath( "#engine/shaders/Raytracing.hlsl" ).c_str() );
    create_info.filename = shader_path.c_str();

    create_info.frequency = RHI::ShaderFrequency::Raygen;
    create_info.entry_point = "VisibilityRGS";
    RHIObjectPtr<RHIShader> raygen_shader = GetRHI().CreateShader( create_info );

    create_info.entry_point = "VisibilityRMS";
    create_info.frequency = RHI::ShaderFrequency::Miss;
    RHIObjectPtr<RHIShader> miss_shader = GetRHI().CreateShader( create_info );

    create_info.entry_point = "VisibilityRCS";
    create_info.frequency = RHI::ShaderFrequency::ClosestHit;
    RHIObjectPtr<RHIShader> closest_hit_shader = GetRHI().CreateShader( create_info );

    RHIRaytracingPipelineInfo rt_pipeline_info = {};
    rt_pipeline_info.raygen_shader = raygen_shader.get();
    rt_pipeline_info.miss_shader = miss_shader.get();
    rt_pipeline_info.closest_hit_shader = closest_hit_shader.get();
    rt_pipeline_info.binding_layout = m_rt_layout.get();

    m_rt_pipeline = GetRHI().CreatePSO( rt_pipeline_info );
}

void Renderer::CreatePrograms()
{
    m_blit_texture_prog = std::make_unique<BlitTextureProgram>();
}

void Renderer::CreateSamplers()
{
    RHI::SamplerInfo info_point = {};
    m_point_sampler = GetRHI().CreateSampler( info_point );
}

void Renderer::CreateDescriptorSetLayout()
{
    {
        RHI::DescriptorViewRange ranges[1] = {};
        ranges[0].type = RHIShaderBindingType::TextureRW;
        ranges[0].count = 1;
        ranges[0].stages = RHIShaderStageFlags::RaygenShader;

        RHI::DescriptorSetLayoutInfo binding_table = {};
        binding_table.ranges = ranges;
        binding_table.range_count = std::size( ranges );

        m_rt_dsl = GetRHI().CreateDescriptorSetLayout( binding_table );
    }

    {
        RHI::DescriptorViewRange ranges[2] = {};
        ranges[0].type = RHIShaderBindingType::AccelerationStructure;
        ranges[0].count = 1;
        ranges[0].stages = RHIShaderStageFlags::AllBits;

        ranges[1].type = RHIShaderBindingType::UniformBuffer;
        ranges[1].count = 1;
        ranges[1].stages = RHIShaderStageFlags::AllBits;

        RHI::DescriptorSetLayoutInfo binding_table = {};
        binding_table.ranges = ranges;
        binding_table.range_count = std::size( ranges );

        m_view_dsl = GetRHI().CreateDescriptorSetLayout( binding_table );
    }

    RHI::ShaderBindingLayoutInfo rt_layout_info = {};

    RHIDescriptorSetLayout* rt_dsls[2] = { m_rt_dsl.get(), m_view_dsl.get() };
    rt_layout_info.tables = rt_dsls;
    rt_layout_info.table_count = std::size( rt_dsls );

    m_rt_layout = GetRHI().CreateShaderBindingLayout( rt_layout_info );
}

void Renderer::UpdateSceneViewParams( const SceneViewFrameData& view_data )
{
    // Fill uniform buffer
    GPUSceneViewParams svp = {};

    const SceneView& view = *view_data.view;

    svp.view_mat = view.CalcViewMatrix();
    svp.proj_mat = view.CalcProjectionMatrix();
    svp.viewport_size_px = view.GetExtent();

    glm::mat4x4 view_proj = svp.proj_mat * svp.view_mat;
    svp.view_proj_inv_mat = glm::inverse( view_proj );

    UploadBufferRange gpu_buffer = view_data.rg->AllocateUploadBuffer( sizeof( GPUSceneViewParams ) );

    gpu_buffer.UploadData( svp );

    // Update descriptor set
    view_data.view_desc_set->BindAccelerationStructure( 0, 0, view.GetScene().GetTLAS().GetRHIAS() );
    view_data.view_desc_set->BindUniformBufferView( 1, 0, gpu_buffer.view );

    view_data.view_desc_set->FlushBinds();
}

RHICommandList* Renderer::CreateInitializedCommandList( RHI::QueueType queue_type ) const
{
    RHICommandList* cmd_list = GetRHI().GetCommandList( queue_type );

    cmd_list->Begin();

    return cmd_list;
}

void Renderer::SetPSO( RHICommandList& cmd_list, const RHIRaytracingPipeline& rt_pso, const SceneViewFrameData& view_data ) const
{
    cmd_list.SetPSO( rt_pso );
    cmd_list.BindDescriptorSet( 1, *view_data.view_desc_set );
}

bool Renderer::RenderScene( const RenderSceneParams& parms )
{
    if ( !SE_ENSURE( parms.view && parms.rg ) )
        return false;

    SceneView& scene_view = *parms.view;
    Scene& scene = scene_view.GetScene();
    Rendergraph& rg = *parms.rg;

    // 1. Setup stage. Setup your resource handles and passes
    RGExternalTextureDesc scene_output_desc = {};
    scene_output_desc.initial_layout = RHITextureLayout::ShaderReadOnly;
    scene_output_desc.final_layout = RHITextureLayout::ShaderReadOnly;

    SceneViewFrameData view_frame_data = {};
    view_frame_data.view = parms.view;
    view_frame_data.rg = parms.rg;
    view_frame_data.view_desc_set = rg.AllocateFrameDescSet( *m_view_dsl );
    view_frame_data.scene_output_idx = 0;

    RGExternalTexture* scene_output[2];
    const RGTextureROView* scene_output_ro_view[2];
    const RGTextureRWView* scene_output_rw_view[2];
    const RGRenderTargetView* scene_output_rt_view[2];
    for ( int i = 0; i < 2; i++ )
    {
        std::string name = "scene_output#";
        name += std::to_string( i );
        scene_output_desc.name = name.c_str();
        scene_output_desc.rhi_texture = scene_view.GetFrameColorTexture( i );
        scene_output[i] = rg.RegisterExternalTexture(scene_output_desc);
        scene_output_ro_view[i] = scene_output[i]->RegisterExternalROView( { scene_view.GetFrameColorTextureROView( i ) } );
        scene_output_rw_view[i] = scene_output[i]->RegisterExternalRWView( { scene_view.GetFrameColorTextureRWView( i ) } );
        scene_output_rt_view[i] = scene_output[i]->RegisterExternalRTView( { scene_view.GetFrameColorTextureRTView( i ) } );
        view_frame_data.scene_output[i] = scene_output[i];
    }   

    RGPass* update_as_pass = rg.AddPass( RHI::QueueType::Graphics, "UpdateAS" );

    RGPass* rt_pass = rg.AddPass( RHI::QueueType::Graphics, "RaytraceScene" );
    rt_pass->UseTextureView( *scene_output_rw_view[view_frame_data.scene_output_idx] );

    DisplayMappingContext display_mapping_ctx = {};
    m_display_mapping->SetupRendergraph( view_frame_data, display_mapping_ctx );

    if ( parms.extension ) {
        parms.extension->PostSetupRendergraph( view_frame_data );
    }

    // 2. Compile stage. That will create a timeline for each resource, allowing us to fetch real RHI handles for them inside passes
    if ( !rg.Compile() )
        return false;

    if ( parms.extension ) {
        parms.extension->PostCompileRendergraph( view_frame_data );
    }

    // 3. Record phase. Fill passes with command lists

    // @todo - taskgraph

    RHICommandList* cmd_list_rt = CreateInitializedCommandList( RHI::QueueType::Graphics );

    update_as_pass->AddCommandList( *cmd_list_rt );
    {
        if ( !scene.GetTLAS().Build( *cmd_list_rt ) )
        {
            SE_LOG_ERROR( Renderer, "Couldn't build a tlas!" );
        }
    }
    update_as_pass->EndPass();

    // We have to update tlas for RHI pointer to be valid
    UpdateSceneViewParams( view_frame_data );

    rt_pass->BorrowCommandList( *cmd_list_rt );
    {
        RHIDescriptorSet* rt_descset = rg.AllocateFrameDescSet( *m_rt_dsl );
        rt_descset->BindTextureRWView( 0, 0, *scene_output[0]->GetRWView()->GetRHIView()); // index must match with rgtexture used on setup stage

        SetPSO( *cmd_list_rt, *m_rt_pipeline, view_frame_data );

        cmd_list_rt->BindDescriptorSet( 0, *rt_descset );

        cmd_list_rt->TraceRays( glm::uvec3( scene_view.GetExtent(), 1 ) );
    }
    rt_pass->EndPass();

    m_display_mapping->DisplayMappingPass( *cmd_list_rt, view_frame_data, display_mapping_ctx );

    cmd_list_rt->End();

    return true;
}

void Renderer::DebugUI()
{
    if ( r_debugDisplayMapping.GetValue() > 0 )
    {
        bool window_open = true;
        if ( ImGui::Begin( "Display mapping debug", &window_open ) )
        {
            m_display_mapping->DebugUI();
        }
        ImGui::End();

        if ( window_open == false )
        {
            r_debugDisplayMapping.SetValue( 0 );
        }
    }
}

BlitTextureProgram* Renderer::GetBlitTextureProgram() const
{
    return m_blit_texture_prog.get();
}

RHISampler* Renderer::GetPointSampler() const
{
    return m_point_sampler.get();
}
