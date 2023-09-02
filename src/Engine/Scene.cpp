#include "StdAfx.h"

#include "Scene.h"

#include "Rendergraph.h"

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
    tex_info.usage = RHITextureUsageFlags::TextureROView | RHITextureUsageFlags::TextureRWView;
    tex_info.initial_layout = RHITextureLayout::ShaderReadOnly;

    m_rt_frame = GetRHI().CreateTexture( tex_info );

    RHI::TextureRWViewInfo uav_info = {};
    uav_info.texture = m_rt_frame.get();
    m_frame_rwview = GetRHI().CreateTextureRWView(uav_info);

    RHI::TextureROViewInfo srv_info = {};
    srv_info.texture = m_rt_frame.get();
    m_frame_roview = GetRHI().CreateTextureROView(srv_info);
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

    CreateSceneViewParamsBuffers();
    CreateDescriptorSetLayout();
    CreateDescriptorSets();
    CreateRTPipeline();
}

void Renderer::NextFrame()
{
    m_frame_idx++;
}

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

void Renderer::CreateSceneViewParamsBuffers()
{
    m_view_buffers.resize( NumBufferizedFrames );
    m_view_buffer_views.resize( NumBufferizedFrames );

    RHI::BufferInfo uniform_info = {};
    uniform_info.size = sizeof( GPUSceneViewParams );
    uniform_info.usage = RHIBufferUsageFlags::UniformBuffer;

    for ( size_t i = 0; i < NumBufferizedFrames; ++i )
    {
        m_view_buffers[i] = GetRHI().CreateUploadBuffer( uniform_info );
        RHI::UniformBufferViewInfo view_info = {};
        view_info.buffer = m_view_buffers[i]->GetBuffer();
        m_view_buffer_views[i] = GetRHI().CreateUniformBufferView( view_info );
    }
}

void Renderer::CreateDescriptorSets()
{
    m_rt_descsets.resize( NumBufferizedFrames );
    for ( size_t i = 0; i < NumBufferizedFrames; ++i )
    {
        m_rt_descsets[i] = GetRHI().CreateDescriptorSet( *m_rt_dsl );
    }

    m_view_descsets.resize( NumBufferizedFrames );
    for ( size_t i = 0; i < NumBufferizedFrames; ++i )
    {
        m_view_descsets[i] = GetRHI().CreateDescriptorSet( *m_view_dsl );
    }
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

void Renderer::UpdateSceneViewParams( const SceneView& scene_view )
{
    // Fill uniform buffer
    GPUSceneViewParams svp = {};

    svp.view_mat = scene_view.CalcViewMatrix();
    svp.proj_mat = scene_view.CalcProjectionMatrix();
    svp.viewport_size_px = scene_view.GetExtent();

    glm::mat4x4 view_proj = svp.proj_mat * svp.view_mat;
    svp.view_proj_inv_mat = glm::inverse( view_proj );

    svp.proj_mat[1][1] *= -1; // ogl -> vulkan y axis

    m_view_buffers[GetCurrFrameBufIndex()]->WriteBytes( &svp, sizeof( GPUSceneViewParams ) );

    // Update descriptor set
    RHIDescriptorSetPtr curr_view_descset = m_view_descsets[GetCurrFrameBufIndex()];
    curr_view_descset->BindAccelerationStructure( 0, 0, scene_view.GetScene().GetTLAS().GetRHIAS() );
    curr_view_descset->BindUniformBufferView( 1, 0, *m_view_buffer_views[GetCurrFrameBufIndex()] );

    curr_view_descset->FlushBinds();
}

uint32_t Renderer::GetCurrFrameBufIndex() const
{
    return m_frame_idx % NumBufferizedFrames;
}

RHICommandList* Renderer::CreateInitializedCommandList( RHI::QueueType queue_type ) const
{
    RHICommandList* cmd_list = GetRHI().GetCommandList( queue_type );

    cmd_list->Begin();

    return cmd_list;
}

void Renderer::SetPSO( RHICommandList& cmd_list, const RHIRaytracingPipeline& rt_pso ) const
{
    cmd_list.SetPSO( rt_pso );
    cmd_list.BindDescriptorSet( 1, *m_view_descsets[GetCurrFrameBufIndex()] );
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
    scene_output_desc.name = "scene_output";
    scene_output_desc.rhi_texture = scene_view.GetFrameColorTexture();
    scene_output_desc.initial_layout = RHITextureLayout::ShaderReadOnly;
    scene_output_desc.final_layout = RHITextureLayout::ShaderReadOnly;

    RGExternalTexture* scene_output = rg.RegisterExternalTexture( scene_output_desc );

    RGPass* update_as_pass = rg.AddPass( RHI::QueueType::Graphics, "UpdateAS" );

    RGPass* rt_pass = rg.AddPass( RHI::QueueType::Graphics, "RaytraceScene" );
    rt_pass->UseTexture( *scene_output, RGTextureUsage::ShaderReadWrite );

    if ( parms.extension ) {
        parms.extension->PostSetupRendergraph( rg, *scene_output );
    }

    // 2. Compile stage. That will create a timeline for each resource, allowing us to fetch real RHI handles for them inside passes
    if ( !rg.Compile() )
        return false;

    if ( parms.extension ) {
        parms.extension->PostCompileRendergraph( rg, *scene_output );
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
    UpdateSceneViewParams( scene_view );

    rt_pass->BorrowCommandList( *cmd_list_rt );
    {
        m_rt_descsets[GetCurrFrameBufIndex()]->BindTextureRWView( 0, 0, *scene_view.GetFrameColorTextureRWView() );

        SetPSO( *cmd_list_rt, *m_rt_pipeline );

        cmd_list_rt->BindDescriptorSet( 0, *m_rt_descsets[GetCurrFrameBufIndex()] );

        cmd_list_rt->TraceRays( glm::uvec3( scene_view.GetExtent(), 1 ) );
    }
    rt_pass->EndPass();

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
            ImGui::Text( "This window will be used for display mapping debugging. Right now its empty" );
        }
        ImGui::End();

        if ( window_open == false )
        {
            r_debugDisplayMapping.SetValue( 0 );
        }
    }
}
