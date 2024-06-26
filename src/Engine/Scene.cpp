#include "StdAfx.h"

#include "Scene.h"

#include "DescriptorSetPool.h"
#include "DisplayMapping.h"
#include "ShaderPrograms.h"
#include "Rendergraph.h"
#include "UploadBufferPool.h"

#include <ImguiBackend/ImguiBackend.h>


CVAR_DEFINE( r_debugDisplayMapping, int, 0, "Show debug interface for display mapping" );

CVAR_DEFINE( r_showStats, int, 0, "Show renderer stats" );

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
    VERIFY_NOT_EQUAL( GetRenderer().GetDebugDrawing().InitSceneViewData( m_debug_draw_data ), false );
}

void SceneView::SetLookAt( const glm::vec3& eye, const glm::vec3& center )
{
    m_eye_pos = eye;
    m_eye_orientation = glm::quatLookAt( glm::normalize( center - eye ), m_up );
}

void SceneView::SetCameraOrientation( const glm::quat& orientation )
{
    m_eye_orientation = orientation;
}

void SceneView::SetCameraPosition( const glm::vec3& eye )
{
    m_eye_pos = eye;
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
    // @todo - rgbe5
    tex_info.format = RHIFormat::RGBA32_SFLOAT;
    tex_info.usage = RHITextureUsageFlags::TextureROView | RHITextureUsageFlags::TextureRWView | RHITextureUsageFlags::RenderTargetView;
    tex_info.initial_layout = RHITextureLayout::ShaderReadOnly;

    for ( int i = 0; i < 2; ++i )
    {
        m_rt_frame[i] = GetRHI().CreateTexture( tex_info );

        RHI::TextureRWViewInfo uav_info = {};
        uav_info.texture = m_rt_frame[i].get();
        m_frame_rwview[i] = GetRHI().CreateTextureRWView( uav_info );

        RHI::TextureROViewInfo srv_info = {};
        srv_info.texture = m_rt_frame[i].get();
        m_frame_roview[i] = GetRHI().CreateTextureROView( srv_info );

        RHI::RenderTargetViewInfo rtv_info = {};
        rtv_info.texture = m_rt_frame[i].get();
        rtv_info.format = tex_info.format;
        m_frame_rtview[i] = GetRHI().CreateRTV( rtv_info );
    }

    RHI::TextureInfo level_obj_id_info = tex_info;
    level_obj_id_info.format = RHIFormat::R16_UINT;
    level_obj_id_info.usage = RHITextureUsageFlags::TextureROView | RHITextureUsageFlags::TextureRWView;

    m_level_objects_id_tex = GetRHI().CreateTexture( level_obj_id_info );

    ResetAccumulation();
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

void SceneView::DebugUI() const
{
    if ( r_showStats.GetValue() > 0 )
    {
        bool window_open = true;
        if ( ImGui::Begin( "Renderer stats", &window_open ) )
        {
            ImGui::Text( "Num samples = %lu", m_num_accumulated_frames );
        }
        ImGui::End();

        if ( window_open == false )
        {
            r_showStats.SetValue( 0 );
        }
    }
}


// GlobalDescriptors

namespace
{
    // must be in sync with SceneViewParams.hlsli
    struct GPUSceneViewParams
    {
        glm::mat4 view_mat;
        glm::mat4 proj_mat;
        glm::mat4 view_proj_inv_mat;
        glm::uvec2 viewport_size_px;
        glm::ivec2 cursor_position_px;
        uint32_t random_uint;
        uint32_t use_accumulation;
        uint32_t pad[2];
    };

    struct GPUTLASItemParams
    {
        glm::mat3x4 object_to_world_mat;
        uint32_t geom_buf_index;
        uint32_t material_index;
        uint32_t picking_id;
    };

    static constexpr int MAX_SCENE_GEOMS = 256;
    static constexpr int MAX_SCENE_MATERIALS = 256;
}

GlobalDescriptors::GlobalDescriptors()
{
    m_free_geom_slots.resize( MAX_SCENE_GEOMS );
    for ( uint32_t i = 0; i < m_free_geom_slots.size(); ++i )
    {
        m_free_geom_slots[i] = uint32_t( m_free_geom_slots.size() ) - i - 1;
    }


    {
        RHI::DescriptorViewRange ranges[3] = {};

        ranges[0].type = RHIShaderBindingType::StructuredBuffer;
        ranges[0].count = MAX_SCENE_GEOMS;
        ranges[0].stages = RHIShaderStageFlags::AllBits;

        ranges[1].type = RHIShaderBindingType::StructuredBuffer;
        ranges[1].count = MAX_SCENE_GEOMS;
        ranges[1].stages = RHIShaderStageFlags::AllBits;

        ranges[2].type = RHIShaderBindingType::StructuredBuffer;
        ranges[2].count = 1;
        ranges[2].stages = RHIShaderStageFlags::AllBits;

        RHI::DescriptorSetLayoutInfo dsl_info = {};
        dsl_info.ranges = ranges;
        dsl_info.range_count = std::size( ranges );

        m_global_dsl = GetRHI().CreateDescriptorSetLayout( dsl_info );
    }

    m_global_descset = GetRHI().CreateDescriptorSet( *m_global_dsl );

    m_free_material_slots.resize( MAX_SCENE_MATERIALS );
    for ( uint32_t i = 0; i < m_free_material_slots.size(); ++i )
    {
        m_free_material_slots[i] = uint32_t( m_free_material_slots.size() ) - i - 1;
    }

    RHI::BufferInfo material_buffer_info = {};
    material_buffer_info.name = "global_material_buf";
    material_buffer_info.size = sizeof( MaterialGPU ) * MAX_SCENE_MATERIALS;
    material_buffer_info.usage = RHIBufferUsageFlags::StructuredBuffer;

    // @todo - this should be device buffer instead of upload, given how often its updated and accessed
    m_material_buffer = GetRHI().CreateUploadBuffer( material_buffer_info );

    RHIBufferViewInfo material_buffer_view_info = {};
    material_buffer_view_info.buffer = m_material_buffer->GetBuffer();
    m_global_descset->BindStructuredBuffer( 2, 0, material_buffer_view_info );
}

uint32_t GlobalDescriptors::AddGeometry( const RHIBufferViewInfo& vertices, const RHIBufferViewInfo& indices )
{
    if ( m_free_geom_slots.empty() )
        return -1; // no place left

    uint32_t geom_index = m_free_geom_slots.back();
    m_free_geom_slots.pop_back();

    m_global_descset->BindStructuredBuffer( 0, geom_index, indices );
    m_global_descset->BindStructuredBuffer( 1, geom_index, vertices );

    return geom_index;
}

uint32_t GlobalDescriptors::AddMaterial( const MaterialGPU& material_data )
{
    if ( m_free_material_slots.empty() )
        return -1; // no place left

    uint32_t material_index = m_free_material_slots.back();
    m_free_material_slots.pop_back();

    m_material_buffer->WriteBytes( &material_data, sizeof( MaterialGPU ), material_index * sizeof( MaterialGPU ) );

    return material_index;
}


// Renderer

Renderer::Renderer()
{
    if ( !GetRHI().SupportsRaytracing() )
    {
        SE_LOG_FATAL_ERROR( Renderer, "Raytracing is not supported on the GPU" );
    }

    m_global_descriptors = std::make_unique<GlobalDescriptors>();

    CreateDescriptorSetLayout();
    CreatePrograms();
    CreateSamplers();
    CreateRTPipeline();

    m_display_mapping = std::make_unique<DisplayMapping>( m_view_dsl.get() );
    m_debug_drawing = std::make_unique<DebugDrawing>( m_view_dsl.get() );

}

Renderer::~Renderer() = default;

bool Renderer::LoadDefaultAssets()
{
    const char* default_texture_path = "#engine/Textures/Missing.sea";
    m_default_texture = LoadAsset<TextureAsset>( default_texture_path );
    if ( m_default_texture == nullptr )
    {
        SE_LOG_FATAL_ERROR( Renderer, "Could not load default texture at %s", m_default_texture );
    }

    const char* default_material_path = "#engine/Materials/Default.sea";
    m_default_material = LoadAsset<MaterialAsset>( default_material_path );
    if ( m_default_material == nullptr )
    {
        SE_LOG_FATAL_ERROR( Renderer, "Could not load default material at %s", default_material_path );
    }
    return true;
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

RHIReadbackBufferPtr Renderer::CreateViewFrameReadbackBuffer() const
{
    RHI::BufferInfo view_frame_readback_info;
    view_frame_readback_info.size = sizeof( ViewFrameReadbackData );
    view_frame_readback_info.usage = RHIBufferUsageFlags::StructuredBuffer;
    view_frame_readback_info.name = "viewFrameReadback";

    return GetRHI().CreateReadbackBuffer( view_frame_readback_info );
}

// ReadbackClearProgram

class ReadbackClearProgram : public ShaderProgram
{
private:
    RHIDescriptorSetLayoutPtr m_dsl;

    RHIShaderPtr m_cs = nullptr;

    RHIComputePipelinePtr m_pso;

public:
    struct Params
    {
        RHIBufferViewInfo readback_buffer_info;
    };

    ReadbackClearProgram()
        : ShaderProgram()
    {
        m_type = ShaderProgramType::Compute;
        {
            RHI::DescriptorViewRange ranges[1] = {};

            ranges[0].type = RHIShaderBindingType::StructuredBuffer;
            ranges[0].count = 1;
            ranges[0].stages = RHIShaderStageFlags::ComputeShader;

            RHI::DescriptorSetLayoutInfo dsl_info = {};
            dsl_info.ranges = ranges;
            dsl_info.range_count = std::size( ranges );

            m_dsl = GetRHI().CreateDescriptorSetLayout( dsl_info );
            VERIFY_NOT_EQUAL( m_dsl, nullptr );

            RHI::ShaderBindingLayoutInfo layout_info = {};
            RHIDescriptorSetLayout* dsls[1] = { m_dsl.get() };
            layout_info.tables = dsls;
            layout_info.table_count = std::size( dsls );

            m_layout = GetRHI().CreateShaderBindingLayout( layout_info );
            VERIFY_NOT_EQUAL( m_layout, nullptr );
        }

        {
            RHI::ShaderCreateInfo create_info = {};
            std::wstring shader_path = ToWString( ToOSPath( "#engine/shaders/Readback.hlsl" ).c_str() );
            create_info.filename = shader_path.c_str();

            create_info.frequency = RHI::ShaderFrequency::Compute;
            create_info.entry_point = "ReadbackDataClearCS";
            m_cs = GetRHI().CreateShader( create_info );
            VERIFY_NOT_EQUAL( m_cs, nullptr );
        }

        {
            RHIComputePipelineInfo pso_info = {};
            pso_info.compute_shader = m_cs.get();
            pso_info.binding_layout = m_layout.get();

            m_pso = GetRHI().CreatePSO( pso_info );
            VERIFY_NOT_EQUAL( m_pso, nullptr );
        }
    }

    bool Run( RHICommandList& cmd_list, Rendergraph& rg, const Params& parms ) const
    {
        if ( !SE_ENSURE( parms.readback_buffer_info.buffer != nullptr ) )
            return false;

        cmd_list.SetPSO( *m_pso );

        RHIDescriptorSet* pass_descset = rg.AllocateFrameDescSet( *m_dsl );

        pass_descset->BindStructuredBuffer( 0, 0, parms.readback_buffer_info );
        cmd_list.BindDescriptorSet( 0, *pass_descset );

        cmd_list.Dispatch( glm::uvec3( 1, 1, 1 ) );

        return true;
    }
};


// ComputeOutlineProgram

class ComputeOutlineProgram : public ShaderProgram
{
private:
    RHIDescriptorSetLayoutPtr m_dsl = nullptr;
    RHIShaderPtr m_cs = nullptr;

    RHIComputePipelinePtr m_pso = nullptr;

public:
    struct Params
    {
        RHITextureRWView* object_id_tex = nullptr;
        RHITextureRWView* dst_tex = nullptr;
        int object_id;
    };

    ComputeOutlineProgram()
        : ShaderProgram()
    {
        m_type = ShaderProgramType::Compute;
        {
            RHI::DescriptorViewRange ranges[3] = {};

            ranges[0].type = RHIShaderBindingType::TextureRW;
            ranges[0].count = 1;
            ranges[0].stages = RHIShaderStageFlags::ComputeShader;

            ranges[1].type = RHIShaderBindingType::TextureRW;
            ranges[1].count = 1;
            ranges[1].stages = RHIShaderStageFlags::ComputeShader;

            ranges[2].type = RHIShaderBindingType::UniformBuffer;
            ranges[2].count = 1;
            ranges[2].stages = RHIShaderStageFlags::ComputeShader;

            RHI::DescriptorSetLayoutInfo dsl_info = {};
            dsl_info.ranges = ranges;
            dsl_info.range_count = std::size( ranges );

            m_dsl = GetRHI().CreateDescriptorSetLayout( dsl_info );
            VERIFY_NOT_EQUAL( m_dsl, nullptr );

            RHI::ShaderBindingLayoutInfo layout_info = {};
            RHIDescriptorSetLayout* dsls[1] = { m_dsl.get() };
            layout_info.tables = dsls;
            layout_info.table_count = std::size( dsls );

            m_layout = GetRHI().CreateShaderBindingLayout( layout_info );
            VERIFY_NOT_EQUAL( m_layout, nullptr );
        }

        {
            RHI::ShaderCreateInfo create_info = {};
            std::wstring shader_path = ToWString( ToOSPath( "#engine/shaders/Outline.hlsl" ).c_str() );
            create_info.filename = shader_path.c_str();

            create_info.frequency = RHI::ShaderFrequency::Compute;
            create_info.entry_point = "ComputeOutlineCS";
            m_cs = GetRHI().CreateShader( create_info );
            VERIFY_NOT_EQUAL( m_cs, nullptr );
        }

        {
            RHIComputePipelineInfo pso_info = {};
            pso_info.compute_shader = m_cs.get();
            pso_info.binding_layout = m_layout.get();

            m_pso = GetRHI().CreatePSO( pso_info );
            VERIFY_NOT_EQUAL( m_pso, nullptr );
        }
    }

    bool Run( RHICommandList& cmd_list, Rendergraph& rg, const Params& parms ) const
    {
        if ( !SE_ENSURE( parms.dst_tex != nullptr ) )
            return false;

        if ( !SE_ENSURE( parms.object_id_tex != nullptr ) )
            return false;

        cmd_list.SetPSO( *m_pso );

        RHIDescriptorSet* pass_descset = rg.AllocateFrameDescSet( *m_dsl );

        struct GPUOutlineParams
        {
            uint32_t object_id;
        } gpu_outline_parms;
        gpu_outline_parms.object_id = parms.object_id;
        UploadBufferRange gpu_buf = rg.AllocateUploadBufferUniform<GPUOutlineParams>();
        gpu_buf.UploadData( gpu_outline_parms );

        pass_descset->BindTextureRWView( 0, 0, *parms.object_id_tex );
        pass_descset->BindTextureRWView( 1, 0, *parms.dst_tex );
        pass_descset->BindUniformBufferView( 2, 0, gpu_buf.view );
        cmd_list.BindDescriptorSet( 0, *pass_descset );

        glm::uvec3 dispatch_size = parms.dst_tex->GetSize();
        dispatch_size.z = 1;
        dispatch_size.x = ( dispatch_size.x + 7 ) / 8;
        dispatch_size.y = ( dispatch_size.y + 7 ) / 8;
        cmd_list.Dispatch( dispatch_size );

        return true;
    }
};

void Renderer::CreatePrograms()
{
    m_blit_texture_prog = std::make_unique<BlitTextureProgram>();
    m_clear_readback_prog = std::make_unique<ReadbackClearProgram>();
    m_compute_outline_prog = std::make_unique<ComputeOutlineProgram>();
}

void Renderer::CreateSamplers()
{
    RHI::SamplerInfo info_point = {};
    m_point_sampler = GetRHI().CreateSampler( info_point );
}

void Renderer::CreateDescriptorSetLayout()
{
    {
        RHI::DescriptorViewRange ranges[2] = {};
        ranges[0].type = RHIShaderBindingType::TextureRW;
        ranges[0].count = 1;
        ranges[0].stages = RHIShaderStageFlags::RaygenShader;

        ranges[1].type = RHIShaderBindingType::TextureRW;
        ranges[1].count = 1;
        ranges[1].stages = RHIShaderStageFlags::RaygenShader;

        RHI::DescriptorSetLayoutInfo binding_table = {};
        binding_table.ranges = ranges;
        binding_table.range_count = std::size( ranges );

        m_rt_dsl = GetRHI().CreateDescriptorSetLayout( binding_table );
    }

    {
        RHI::DescriptorViewRange ranges[8] = {};
        ranges[0].type = RHIShaderBindingType::AccelerationStructure;
        ranges[0].count = 1;
        ranges[0].stages = RHIShaderStageFlags::AllBits;

        ranges[1].type = RHIShaderBindingType::UniformBuffer;
        ranges[1].count = 1;
        ranges[1].stages = RHIShaderStageFlags::AllBits;

        ranges[2].type = RHIShaderBindingType::StructuredBuffer;
        ranges[2].count = 1;
        ranges[2].stages = RHIShaderStageFlags::AllBits;

        ranges[3].type = RHIShaderBindingType::StructuredBuffer;
        ranges[3].count = 1;
        ranges[3].stages = RHIShaderStageFlags::AllBits;

        ranges[4].type = RHIShaderBindingType::StructuredBuffer;
        ranges[4].count = 1;
        ranges[4].stages = RHIShaderStageFlags::AllBits;

        ranges[5].type = RHIShaderBindingType::StructuredBuffer;
        ranges[5].count = 1;
        ranges[5].stages = RHIShaderStageFlags::AllBits;

        ranges[6].type = RHIShaderBindingType::TextureRO;
        ranges[6].count = 1;
        ranges[6].stages = RHIShaderStageFlags::AllBits;

        ranges[7].type = RHIShaderBindingType::Sampler;
        ranges[7].count = 1;
        ranges[7].stages = RHIShaderStageFlags::AllBits;

        RHI::DescriptorSetLayoutInfo binding_table = {};
        binding_table.ranges = ranges;
        binding_table.range_count = std::size( ranges );

        m_view_dsl = GetRHI().CreateDescriptorSetLayout( binding_table );
    }

    RHI::ShaderBindingLayoutInfo rt_layout_info = {};

    RHIDescriptorSetLayout* rt_dsls[3] = { m_rt_dsl.get(), m_view_dsl.get(), m_global_descriptors->GetLayout() };
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

    svp.cursor_position_px = glm::ivec2( view.GetCursorPosition() );

    m_random_seed = uint32_t( view.m_num_accumulated_frames );

    svp.random_uint = HashUint32( m_random_seed++ );

    svp.use_accumulation = view_data.accumulated_idx != -1 ? 1 : 0;

    UploadBufferRange gpu_buffer = view_data.rg->AllocateUploadBufferUniform<GPUSceneViewParams>();

    gpu_buffer.UploadData( svp );

    auto& tlas_instances = view.GetScene().GetTLAS().Instances();

    std::vector<GPUTLASItemParams> tlas_instances_data( tlas_instances.size() );
    for ( size_t i = 0; i < tlas_instances.size(); ++i )
    {
        tlas_instances_data[i].object_to_world_mat = tlas_instances.data()[i].transform;
    }

    for ( const auto& mesh_instance : view.GetScene().GetAllMeshInstances() )
    {
        size_t packed_idx = tlas_instances.get_packed_idx( mesh_instance.m_tlas_instance );
        tlas_instances_data[packed_idx].geom_buf_index = mesh_instance.m_asset->GetGlobalGeomIndex();
        tlas_instances_data[packed_idx].material_index = -1;
        const MaterialAsset* material = mesh_instance.m_asset->GetMaterial();
        if ( material == nullptr )
        {
            material = m_default_material.get();
        }
        tlas_instances_data[packed_idx].material_index = material->GetGlobalMaterialIndex();
        tlas_instances_data[packed_idx].picking_id = mesh_instance.picking_id;
    }

    UploadBufferRange gpu_tlas_item_params = view_data.rg->AllocateUploadBufferStructured<GPUTLASItemParams>( tlas_instances_data.size() );
    gpu_tlas_item_params.UploadData( tlas_instances_data.data(), tlas_instances_data.size() );

    // Update descriptor set
    view_data.view_desc_set->BindAccelerationStructure( 0, 0, view.GetScene().GetTLAS().GetRHIAS() );
    view_data.view_desc_set->BindUniformBufferView( 1, 0, gpu_buffer.view );
    view_data.view_desc_set->BindStructuredBuffer( 2, 0, gpu_tlas_item_params.view );
    view_data.view_desc_set->BindStructuredBuffer( 3, 0, RHIBufferViewInfo{ view.GetDebugDrawData().m_lines_buf.get() } );
    view_data.view_desc_set->BindStructuredBuffer( 4, 0, RHIBufferViewInfo{ view.GetDebugDrawData().m_lines_indirect_args_buf.get() } );
    view_data.view_desc_set->BindStructuredBuffer( 5, 0, RHIBufferViewInfo{ view_data.readback_buf } );

    TextureAsset* env_cubemap_tex = view.GetScene().GetEnvCubemap();
    if ( !env_cubemap_tex )
    {
        env_cubemap_tex = m_default_texture.get();
    }
    view_data.view_desc_set->BindTextureROView( 6, 0, *env_cubemap_tex->GetTextureROView() );

    view_data.view_desc_set->BindSampler( 7, 0, *m_point_sampler );

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
    cmd_list.BindDescriptorSet( 2, m_global_descriptors->GetDescSet() );
}

bool Renderer::RenderScene( const RenderSceneParams& parms )
{
    if ( !SE_ENSURE( parms.view && parms.rg ) )
        return false;

    m_global_descriptors->GetDescSet().FlushBinds();

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
    view_frame_data.accumulated_idx = parms.view->GetAccumulatedTextureIdx();
    view_frame_data.scene_output_idx = view_frame_data.accumulated_idx == -1 ? 0 : view_frame_data.accumulated_idx;
    view_frame_data.readback_buf = parms.readback_buffer;

    RGExternalTexture* scene_output[2];
    const RGTextureROView* scene_output_ro_view[2];
    const RGRenderTargetView* scene_output_rt_view[2];
    for ( int i = 0; i < 2; i++ )
    {
        std::string name = "scene_output#";
        name += std::to_string( i );
        scene_output_desc.name = name.c_str();
        scene_output_desc.rhi_texture = scene_view.GetFrameColorTexture( i );
        scene_output[i] = rg.RegisterExternalTexture(scene_output_desc);
        scene_output_ro_view[i] = scene_output[i]->RegisterExternalROView( { scene_view.GetFrameColorTextureROView( i ) } );
        scene_output_rt_view[i] = scene_output[i]->RegisterExternalRTView( { scene_view.GetFrameColorTextureRTView( i ) } );
        view_frame_data.scene_output[i] = scene_output[i];
    }   

    RGPass* update_as_pass = rg.AddPass( RHI::QueueType::Graphics, "UpdateAS" );

    DebugDrawingContext debug_drawing_ctx = {};
    m_debug_drawing->AddInitPasses( view_frame_data, debug_drawing_ctx );

    if ( !SE_ENSURE( parms.readback_buffer ) )
    {
        // @todo - set some dummy transient buffer
        return false;
    }

    RGExternalBufferDesc buf_desc = {};
    buf_desc.name = "view_frame_readback";
    buf_desc.rhi_buffer = parms.readback_buffer;
    RGExternalBuffer* readback_buf = parms.rg->RegisterExternalBuffer( buf_desc );

    RGPass* initialize_readback_pass = rg.AddPass( RHI::QueueType::Graphics, "InitializeReadback" );
    initialize_readback_pass->UseBuffer( *readback_buf, RGBufferUsage::ShaderReadWrite );

    RGPass* rt_pass = rg.AddPass( RHI::QueueType::Graphics, "RaytraceScene" );
    rt_pass->UseTextureView( *scene_output[view_frame_data.scene_output_idx]->GetRWView() );

    RGExternalTextureDesc level_objects_id_desc = {};
    level_objects_id_desc.initial_layout = RHITextureLayout::ShaderReadOnly;
    level_objects_id_desc.final_layout = RHITextureLayout::ShaderReadOnly;
    level_objects_id_desc.name = "level_objects_id";
    level_objects_id_desc.rhi_texture = parms.view->GetLevelObjIdTexture();
    RGExternalTexture* level_objects_id = rg.RegisterExternalTexture( level_objects_id_desc );
    rt_pass->UseTextureView( *level_objects_id->GetRWView() );

    DisplayMappingContext display_mapping_ctx = {};
    m_display_mapping->SetupRendergraph( view_frame_data, display_mapping_ctx );

    RGPass* outline_pass = nullptr;
    const RGTextureRWView* outline_pass_dst_view = nullptr;
    if ( parms.outline_id >= 0 )
    {
        outline_pass = rg.AddPass( RHI::QueueType::Graphics, "DrawOutline" );
        outline_pass_dst_view = view_frame_data.scene_output[view_frame_data.scene_output_idx]->GetRWView();
        outline_pass->UseTextureView( *view_frame_data.scene_output[view_frame_data.scene_output_idx]->GetRWView() );
        outline_pass->UseTextureView( *level_objects_id->GetRWView() );
    }

    m_debug_drawing->AddDrawPasses( view_frame_data, debug_drawing_ctx );

    if ( parms.extension )
    {
        parms.extension->PostSetupRendergraph( view_frame_data );
    }

    // 2. Compile stage. That will create a timeline for each resource, allowing us to fetch real RHI handles for them inside passes
    if ( !rg.Compile() )
        return false;

    if ( parms.extension )
    {
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

    m_debug_drawing->RecordInitPasses( *cmd_list_rt, view_frame_data, debug_drawing_ctx );

    SE_ENSURE( initialize_readback_pass->BorrowCommandList( *cmd_list_rt ) );
    {
        ReadbackClearProgram::Params readback_prog_parms = {};
        readback_prog_parms.readback_buffer_info.buffer = parms.readback_buffer;
        m_clear_readback_prog->Run( *cmd_list_rt, *parms.rg, readback_prog_parms );
    }
    initialize_readback_pass->EndPass();

    SE_ENSURE( rt_pass->BorrowCommandList( *cmd_list_rt ) );
    {
        RHIDescriptorSet* rt_descset = rg.AllocateFrameDescSet( *m_rt_dsl );
        rt_descset->BindTextureRWView( 0, 0, *scene_output[view_frame_data.accumulated_idx == -1 ? 0 : view_frame_data.accumulated_idx]->GetRWView()->GetRHIView()); // index must match with rgtexture used on setup stage
        rt_descset->BindTextureRWView( 0, 1, *level_objects_id->GetRWView()->GetRHIView() );

        SetPSO( *cmd_list_rt, *m_rt_pipeline, view_frame_data );

        cmd_list_rt->BindDescriptorSet( 0, *rt_descset );

        cmd_list_rt->TraceRays( glm::uvec3( scene_view.GetExtent(), 1 ) );
    }
    parms.view->SetAccumulatedTextureIdx( view_frame_data.accumulated_idx == -1 ? 0 : view_frame_data.accumulated_idx );
    rt_pass->EndPass();

    m_display_mapping->DisplayMappingPass( *cmd_list_rt, view_frame_data, display_mapping_ctx );

    if ( outline_pass != nullptr )
    {
        outline_pass->BorrowCommandList( *cmd_list_rt );
        ComputeOutlineProgram::Params outline_parms = {};
        outline_parms.object_id_tex = level_objects_id->GetRWView()->GetRHIView();
        outline_parms.dst_tex = outline_pass_dst_view->GetRHIView();
        outline_parms.object_id = parms.outline_id;
        m_compute_outline_prog->Run( *cmd_list_rt, rg, outline_parms );
        outline_pass->EndPass();
    }

    m_debug_drawing->RecordDrawPasses( *cmd_list_rt, view_frame_data, debug_drawing_ctx );

    cmd_list_rt->End();

    scene_view.OnEndRenderFrame();

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
