// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "SceneManager.h"

#include "StaticMeshManager.h"

#include <DirectXMath.h>

StaticMeshID SceneClientView::LoadStaticMesh( std::string name, std::vector<Vertex> vertices, std::vector<uint32_t> indices )
{
    StaticMeshID mesh_id = m_scene->AddStaticMesh();
    m_static_mesh_manager->LoadStaticMesh( mesh_id, std::move( name ),
                                           make_span( vertices ),
                                           make_span( indices ) );

    auto* mesh = m_scene->TryModifyStaticMesh( mesh_id );
    mesh->Vertices() = std::move( vertices );
    mesh->Indices() = std::move( indices );
    mesh->Topology() = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    return mesh_id;
}

TextureID SceneClientView::LoadStreamedTexture( std::string path )
{
    TextureID tex_id = m_scene->AddTexture();
    m_tex_streamer->LoadStreamedTexture( tex_id, std::move( path ) );
    return tex_id;
}

TextureID SceneClientView::LoadStaticTexture( std::string path )
{
    TextureID tex_id = m_scene->AddTexture();
    m_static_texture_manager->LoadTexture( tex_id, std::move( path ) );
    return tex_id;
}

CubemapID SceneClientView::LoadCubemap( std::string path )
{
    CubemapID id = m_scene->AddCubemap();
    m_cubemap_manager->LoadCubemap( id, std::move( path ) );
    return id;
}

CubemapID SceneClientView::AddCubemapFromTexture( TextureID tex_id )
{
    CubemapID cm_id = m_scene->AddCubemap();
    m_cubemap_manager->CreateCubemapFromTexture( cm_id, tex_id );
    return cm_id;
}

TransformID SceneClientView::AddTransform( const DirectX::XMFLOAT4X4& obj2world )
{
    TransformID tf_id = m_scene->AddTransform();
    m_scene->TryModifyTransform( tf_id )->ModifyMat() = obj2world;

    m_dynamic_buffers->AddTransform( tf_id );
    return tf_id;
}

MaterialID SceneClientView::AddMaterial( const MaterialPBR::TextureIds& textures, const DirectX::XMFLOAT3& diffuse_fresnel, const DirectX::XMFLOAT4X4& uv_transform )
{
    MaterialID mat_id = m_scene->AddMaterial( textures );

    MaterialPBR::Data& material_data = m_scene->TryModifyMaterial( mat_id )->Modify();
    material_data.diffuse_fresnel = diffuse_fresnel;
    material_data.transform = uv_transform;

    m_dynamic_buffers->AddMaterial( mat_id );

    m_material_table_baker->RegisterMaterial( mat_id );

    return mat_id;
}

StaticSubmeshID SceneClientView::AddSubmesh( StaticMeshID mesh_id, const StaticSubmesh::Data& data )
{
    StaticSubmeshID id = m_scene->AddStaticSubmesh( mesh_id );
    m_scene->TryModifyStaticSubmesh( id )->Modify() = data;
    return id;
}

MeshInstanceID SceneClientView::AddMeshInstance( StaticSubmeshID submesh_id, TransformID tf_id, MaterialID mat_id )
{
    return m_scene->AddStaticMeshInstance( tf_id, submesh_id, mat_id );
}

EnvMapID SceneClientView::AddEnviromentMap( CubemapID cubemap_id, TransformID transform_id )
{
    EnvMapID id = m_scene->AddEnviromentMap( cubemap_id, transform_id );

    m_material_table_baker->RegisterEnvMap( id );

    return id;
}

EnviromentMap* SceneClientView::ModifyEnviromentMap( EnvMapID envmap_id ) noexcept
{
    return m_scene->TryModifyEnvMap( envmap_id );
}

const EnviromentMap* SceneClientView::GetEnviromentMap( EnvMapID envmap_id ) const noexcept
{
    return m_scene->AllEnviromentMaps().try_get( envmap_id );
}

CameraID SceneClientView::AddCamera( const Camera::Data& data ) noexcept
{
    CameraID id = m_scene->AddCamera();
    m_scene->TryModifyCamera(id)->ModifyData() = data;
    return id;
}

const Camera* SceneClientView::GetCamera( CameraID id ) const noexcept
{
    return m_scene->AllCameras().try_get( id );
}

Camera* SceneClientView::ModifyCamera( CameraID id ) noexcept
{
    return m_scene->TryModifyCamera( id );
}

LightID SceneClientView::AddLight( const SceneLight::Data& data ) noexcept
{
    LightID id = m_scene->AddLight();
    m_scene->TryModifyLight( id )->ModifyData() = data;
    return id;
}

const SceneLight* SceneClientView::GetLight( LightID id ) const noexcept
{
    return m_scene->AllLights().try_get( id );
}

SceneLight* SceneClientView::ModifyLight( LightID id ) noexcept
{
    return m_scene->TryModifyLight( id );
}

StaticMeshInstance* SceneClientView::ModifyInstance( MeshInstanceID id ) noexcept
{
    return m_scene->TryModifyStaticMeshInstance( id );
}

ObjectTransform* SceneClientView::ModifyTransform( TransformID id ) noexcept
{
    return m_scene->TryModifyTransform( id );
}



SceneManager::SceneManager( Microsoft::WRL::ComPtr<ID3D12Device> device,
                            size_t nframes_to_buffer, GPUTaskQueue* copy_queue, GPUTaskQueue* graphics_queue )
    : m_static_mesh_mgr( device, &m_scene )
    , m_tex_streamer( device, 700*1024*1024ul, 128*1024*1024ul, nframes_to_buffer, &m_scene )
    , m_static_texture_mgr( device, &m_scene )
    , m_dynamic_buffers( device, &m_scene, nframes_to_buffer )
    , m_scene_view( &m_scene, &m_static_mesh_mgr, &m_static_texture_mgr, &m_tex_streamer, &m_cubemap_mgr, &m_dynamic_buffers, &m_material_table_baker )
    , m_copy_queue( copy_queue )
    , m_nframes_to_buffer( nframes_to_buffer )
    , m_gpu_descriptor_tables( device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, nframes_to_buffer )
    , m_cubemap_mgr( device, &m_gpu_descriptor_tables, &m_scene )
    , m_material_table_baker( device, &m_scene, &m_gpu_descriptor_tables )
    , m_uv_density_calculator( &m_scene )
    , m_graphics_queue( graphics_queue )
{
    for ( size_t i = 0; i < size_t( m_nframes_to_buffer ); ++i )
    {
        m_copy_cmd_allocators.emplace_back();
        auto& allocator = m_copy_cmd_allocators.back();
        ThrowIfFailedH( device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_COPY,
            IID_PPV_ARGS( allocator.first.GetAddressOf() ) ) );
        allocator.second = 0;

        m_graphics_cmd_allocators.emplace_back();
        auto& graphics_allocator = m_graphics_cmd_allocators.back();
        ThrowIfFailedH( device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS( graphics_allocator.first.GetAddressOf() ) ) );
        graphics_allocator.second = 0;
    }

    ThrowIfFailedH( device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_COPY,
        m_copy_cmd_allocators[0].first.Get(), // Associated command allocator
        nullptr,                 
        IID_PPV_ARGS( m_copy_cmd_list.GetAddressOf() ) ) );

    ThrowIfFailedH( device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_graphics_cmd_allocators[0].first.Get(), // Associated command allocator
        nullptr,                   // Initial FramegraphStateObject
        IID_PPV_ARGS( m_graphics_cmd_list.GetAddressOf() ) ) );

    // Later we will call Reset on cmd_list, which demands for the list to be closed
    m_copy_cmd_list->Close();
    m_graphics_cmd_list->Close();
}

const SceneClientView& SceneManager::GetScene() const noexcept
{
    return m_scene_view;
}

SceneClientView& SceneManager::GetScene() noexcept
{
    return m_scene_view;
}

const DescriptorTableBakery& SceneManager::GetDescriptorTables() const noexcept
{
    return m_gpu_descriptor_tables;
}

DescriptorTableBakery& SceneManager::GetDescriptorTables() noexcept
{
    return m_gpu_descriptor_tables;
}

void SceneManager::UpdateFramegraphBindings( CameraID main_camera_id, const ParallelSplitShadowMapping& pssm, const D3D12_VIEWPORT& main_viewport )
{
    SceneCopyOp cur_op = m_operation_counter++;

    m_copy_queue->WaitForTimestamp( m_copy_cmd_allocators[cur_op % m_nframes_to_buffer].second );
    ThrowIfFailedH( m_copy_cmd_allocators[cur_op % m_nframes_to_buffer].first->Reset() );
    m_copy_cmd_list->Reset( m_copy_cmd_allocators[cur_op % m_nframes_to_buffer].first.Get(), nullptr );

    m_graphics_queue->WaitForTimestamp( m_graphics_cmd_allocators[cur_op % m_nframes_to_buffer].second );
    ThrowIfFailedH( m_graphics_cmd_allocators[cur_op % m_nframes_to_buffer].first->Reset() );
    m_graphics_cmd_list->Reset( m_graphics_cmd_allocators[cur_op % m_nframes_to_buffer].first.Get(), nullptr );

    m_main_camera_id = main_camera_id;

    GPUTaskQueue::Timestamp current_copy_time = m_copy_queue->GetCurrentTimestamp();		

    m_static_mesh_mgr.Update( cur_op, current_copy_time, *m_copy_cmd_list.Get() );
    ProcessSubmeshes();
    m_uv_density_calculator.Update( main_camera_id, main_viewport );
    m_tex_streamer.Update( cur_op, current_copy_time, *m_copy_queue, *m_copy_cmd_list.Get() );
    m_static_texture_mgr.Update( cur_op, current_copy_time, *m_copy_cmd_list.Get() );
    m_cubemap_mgr.Update( current_copy_time, cur_op, *m_copy_cmd_list.Get() );
    m_dynamic_buffers.Update();
    m_material_table_baker.UpdateStagingDescriptors();

    ThrowIfFailedH( m_copy_cmd_list->Close() );
    ID3D12CommandList* lists_to_exec[]{ m_copy_cmd_list.Get() };
    m_copy_queue->GetCmdQueue()->ExecuteCommandLists( 1, lists_to_exec );
    
    m_last_copy_timestamp = m_copy_queue->CreateTimestamp();
    m_copy_cmd_allocators[cur_op % m_nframes_to_buffer].second = m_last_copy_timestamp;

    m_static_mesh_mgr.PostTimestamp( cur_op, m_last_copy_timestamp );
    m_static_texture_mgr.PostTimestamp( cur_op, m_last_copy_timestamp );
    m_tex_streamer.PostTimestamp( cur_op, m_last_copy_timestamp );
    m_cubemap_mgr.PostTimestamp( cur_op, m_last_copy_timestamp );

    if ( m_gpu_descriptor_tables.BakeGPUTables() )
    {
        ID3D12DescriptorHeap* heaps[] = { m_gpu_descriptor_tables.CurrentGPUHeap().Get() };
        m_graphics_cmd_list->SetDescriptorHeaps( 1, heaps );

        m_material_table_baker.UpdateGPUDescriptors();
        m_cubemap_mgr.OnBakeDescriptors( *m_graphics_cmd_list.Get() );
    }

    ThrowIfFailedH( m_graphics_cmd_list->Close() );
    lists_to_exec[0] = m_graphics_cmd_list.Get();
    m_graphics_queue->GetCmdQueue()->ExecuteCommandLists( 1, lists_to_exec );

    CleanModifiedItemsStatus();
}


void SceneManager::FlushAllOperations()
{
    m_copy_queue->WaitForTimestamp( m_last_copy_timestamp );
}


void SceneManager::CleanModifiedItemsStatus()
{
    for ( auto& tf : m_scene.TransformSpan() )
        tf.Clean();

    for ( auto& mat : m_scene.MaterialSpan() )
        mat.Clean();

    for ( auto& tex : m_scene.TextureSpan() )
        tex.Clean();

    for ( auto& submesh : m_scene.StaticSubmeshSpan() )
        submesh.Clean();
}


void SceneManager::ProcessSubmeshes()
{
    for ( auto& submesh : m_scene.StaticSubmeshSpan() )
    {
        if ( submesh.IsDirty() )
        {
            CalcSubmeshBoundingBox( submesh );
            m_uv_density_calculator.CalcUVDensityInObjectSpace( submesh );
        }
    }
}

void SceneManager::CalcSubmeshBoundingBox( StaticSubmesh& submesh )
{
    assert( m_scene.AllStaticMeshes().has( submesh.GetMesh() ) );

    const StaticMesh& mesh = m_scene.AllStaticMeshes()[submesh.GetMesh()];
    assert( ( ! mesh.Vertices().empty() && ! mesh.Indices().empty() && submesh.DrawArgs().idx_cnt > 1 ) );

    auto get_vertex = [&]( size_t index_in_submesh ) -> DirectX::XMVECTOR
    {
        return XMLoadFloat3( &mesh.Vertices()[submesh.DrawArgs().base_vertex_loc + mesh.Indices()[submesh.DrawArgs().start_index_loc + index_in_submesh]].pos );
    };
    DirectX::XMVECTOR lo = get_vertex(0);
    DirectX::XMVECTOR hi = lo;

    for ( size_t i = 1; i < submesh.DrawArgs().idx_cnt; ++i )
    {
        DirectX::XMVECTOR v = get_vertex( i );
        lo = DirectX::XMVectorMin( v, lo );
        hi = DirectX::XMVectorMax( v, hi );
    }

    DirectX::BoundingBox::CreateFromPoints( submesh.Box(), hi, lo );
}
