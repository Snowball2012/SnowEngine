// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "SceneManager.h"

#include "StaticMeshManager.h"

#include <DirectXMath.h>

#include "resources/GPUDevice.h"

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

    return tf_id;
}

MaterialID SceneClientView::AddMaterial( const MaterialPBR::TextureIds& textures, const DirectX::XMFLOAT3& diffuse_fresnel, const DirectX::XMFLOAT3& albedo_color, const DirectX::XMFLOAT4X4& uv_transform )
{
    MaterialID mat_id = m_scene->AddMaterial( textures );

    MaterialPBR::Data& material_data = m_scene->TryModifyMaterial( mat_id )->Modify();
    material_data.diffuse_fresnel = diffuse_fresnel;
    material_data.transform = uv_transform;
	material_data.albedo_color = DirectX::XMFLOAT4(
		albedo_color.x, albedo_color.y, albedo_color.z, 1.0f );

    m_dynamic_buffers->AddMaterial( mat_id );

    m_material_table_baker->RegisterMaterial( mat_id );

    return mat_id;
}

MaterialPBR* SceneClientView::ModifyMaterial( MaterialID mat ) noexcept
{
    return m_scene->TryModifyMaterial( mat );
}

StaticSubmeshID SceneClientView::AddSubmesh( StaticMeshID mesh_id, const StaticSubmesh_Deprecated::Data& data )
{
    StaticSubmeshID id = m_scene->AddStaticSubmesh( mesh_id );
    m_scene->TryModifyStaticSubmesh( id )->Modify() = data;
    return id;
}

EnvMapID SceneClientView::AddEnviromentMap( CubemapID cubemap_id, TransformID transform_id )
{
    EnvMapID id = m_scene->AddEnviromentMap( cubemap_id, transform_id );

    m_material_table_baker->RegisterEnvMap( id );

    return id;
}

EnvironmentMap* SceneClientView::ModifyEnviromentMap( EnvMapID envmap_id ) noexcept
{
    return m_scene->TryModifyEnvMap( envmap_id );
}

const EnvironmentMap* SceneClientView::GetEnviromentMap( EnvMapID envmap_id ) const noexcept
{
    return m_scene->AllEnviromentMaps().try_get( envmap_id );
}

LightID SceneClientView::AddLight( const Light::Data& data ) noexcept
{
    LightID id = m_scene->AddLight();
    m_scene->TryModifyLight( id )->ModifyData() = data;
    return id;
}

const Light* SceneClientView::GetLight( LightID id ) const noexcept
{
    return m_scene->AllLights().try_get( id );
}

Light* SceneClientView::ModifyLight( LightID id ) noexcept
{
    return m_scene->TryModifyLight( id );
}

span<Light> SceneClientView::GetAllLights() noexcept
{
    return m_scene->LightSpan();
}

ObjectTransform* SceneClientView::ModifyTransform( TransformID id ) noexcept
{
    return m_scene->TryModifyTransform( id );
}

SceneManager::SceneManager( GPUDevice* device,
                            size_t nframes_to_buffer, GPUTaskQueue* copy_queue, GPUTaskQueue* graphics_queue )
    : m_static_mesh_mgr( device->GetNativeDevice(), &m_scene )
    , m_tex_streamer( device->GetNativeDevice(), 700*1024*1024ul, 32*1024*1024ul, nframes_to_buffer, &m_scene )
    , m_static_texture_mgr( device->GetNativeDevice(), &m_scene )
    , m_dynamic_buffers( device->GetNativeDevice(), &m_scene, nframes_to_buffer )
    , m_scene_view( &m_scene, &m_static_mesh_mgr, &m_static_texture_mgr, &m_tex_streamer, &m_cubemap_mgr, &m_dynamic_buffers, &m_material_table_baker )
    , m_copy_queue( copy_queue )
    , m_nframes_to_buffer( nframes_to_buffer )
    , m_gpu_descriptor_tables( device->GetNativeDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, nframes_to_buffer )
    , m_cubemap_mgr( device->GetNativeDevice(), &m_gpu_descriptor_tables, &m_scene )
    , m_material_table_baker( device->GetNativeDevice(), &m_scene, &m_gpu_descriptor_tables )
    , m_uv_density_calculator( &m_scene )
    , m_graphics_queue( graphics_queue )
    , m_device( device )
{    
    for ( size_t i = 0; i < size_t( m_nframes_to_buffer ); ++i )
    {
        m_copy_cmd_allocators.emplace_back();
        auto& allocator = m_copy_cmd_allocators.back();
        ThrowIfFailedH( device->GetNativeDevice()->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_COPY,
            IID_PPV_ARGS( allocator.first.GetAddressOf() ) ) );
        allocator.second = 0;

        m_graphics_cmd_allocators.emplace_back();
        auto& graphics_allocator = m_graphics_cmd_allocators.back();
        ThrowIfFailedH( device->GetNativeDevice()->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS( graphics_allocator.first.GetAddressOf() ) ) );
        graphics_allocator.second = 0;
    }

    ThrowIfFailedH( device->GetNativeDevice()->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_COPY,
        m_copy_cmd_allocators[0].first.Get(), // Associated command allocator
        nullptr,                 
        IID_PPV_ARGS( m_copy_cmd_list.GetAddressOf() ) ) );

    ThrowIfFailedH( device->GetNativeDevice()->CreateCommandList(
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

void SceneManager::UpdateFramegraphBindings( World::Entity main_camera, const ParallelSplitShadowMapping& pssm, const D3D12_VIEWPORT& main_viewport )
{
    OPTICK_EVENT();
    SceneCopyOp cur_op = m_operation_counter++;

    m_copy_queue->WaitForTimestamp( m_copy_cmd_allocators[cur_op % m_nframes_to_buffer].second );
    ThrowIfFailedH( m_copy_cmd_allocators[cur_op % m_nframes_to_buffer].first->Reset() );
    m_copy_cmd_list->Reset( m_copy_cmd_allocators[cur_op % m_nframes_to_buffer].first.Get(), nullptr );

    m_graphics_queue->WaitForTimestamp( m_graphics_cmd_allocators[cur_op % m_nframes_to_buffer].second );
    ThrowIfFailedH( m_graphics_cmd_allocators[cur_op % m_nframes_to_buffer].first->Reset() );
    m_graphics_cmd_list->Reset( m_graphics_cmd_allocators[cur_op % m_nframes_to_buffer].first.Get(), nullptr );

    const auto* camera_component = m_scene.world.GetComponent<Camera>( main_camera );
	if ( ! SE_ENSURE( camera_component ) )
		throw SnowEngineException( "camera entity doesn't have a camera component" );

    GPUTaskQueue::Timestamp current_copy_time = m_copy_queue->GetCurrentTimestamp();		

    m_static_mesh_mgr.Update( cur_op, current_copy_time, *m_copy_cmd_list.Get(), *m_graphics_cmd_list.Get() );
    ProcessSubmeshes(*m_graphics_cmd_list.Get());
    m_uv_density_calculator.Update( *camera_component, main_viewport );
    m_tex_streamer.Update( cur_op, current_copy_time, *m_copy_queue, *m_copy_cmd_list.Get() );
    m_static_texture_mgr.Update( cur_op, current_copy_time, *m_copy_cmd_list.Get() );
    m_cubemap_mgr.Update( current_copy_time, cur_op, *m_copy_cmd_list.Get() );
    m_dynamic_buffers.Update();
    m_material_table_baker.UpdateStagingDescriptors();

    ThrowIfFailedH( m_copy_cmd_list->Close() );
    ID3D12CommandList* lists_to_exec[]{ m_copy_cmd_list.Get() };
    m_copy_queue->GetNativeQueue()->ExecuteCommandLists( 1, lists_to_exec );
    
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
    m_graphics_queue->GetNativeQueue()->ExecuteCommandLists( 1, lists_to_exec );

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


void SceneManager::ProcessSubmeshes(IGraphicsCommandList& cmd_list)
{
    OPTICK_EVENT();
    
    for ( auto& submesh : m_scene.StaticSubmeshSpan() )
    {
        if ( submesh.IsDirty() )
        {
            CalcSubmeshBoundingBox( submesh );
            m_uv_density_calculator.CalcUVDensityInObjectSpace( submesh );
        }
        if ( m_device->GetNativeRTDevice() && !submesh.GetBLAS() && submesh.GetMesh() != StaticMeshID::nullid )
        {
            const auto& source_mesh = m_scene.AllStaticMeshes()[submesh.GetMesh()];
            if (source_mesh.IsLoaded())
            {
                CreateBLAS(cmd_list, submesh, source_mesh);
            }
        }
    }
}

void SceneManager::CreateBLAS(IGraphicsCommandList& cmd_list, StaticSubmesh_Deprecated& submesh, const StaticMesh& source_mesh)
{
    D3D12_RAYTRACING_GEOMETRY_DESC geometry = {};
    geometry.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometry.Triangles.VertexBuffer.StartAddress = source_mesh.VertexBufferView().BufferLocation + submesh.DrawArgs().base_vertex_loc * source_mesh.VertexBufferView().StrideInBytes;
    geometry.Triangles.VertexBuffer.StrideInBytes = source_mesh.VertexBufferView().StrideInBytes;
    geometry.Triangles.VertexCount = UINT(source_mesh.Vertices().size());
    geometry.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geometry.Triangles.IndexBuffer = source_mesh.IndexBufferView().BufferLocation + submesh.DrawArgs().start_index_loc * sizeof(uint32_t);
    geometry.Triangles.IndexCount = submesh.DrawArgs().idx_cnt;
    geometry.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
    geometry.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS as_inputs = {};
    as_inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    as_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    as_inputs.pGeometryDescs = &geometry;
    as_inputs.NumDescs = 1;
    as_inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO as_build_info = {};
    auto* rt_device = m_device->GetNativeRTDevice();
    rt_device->GetRaytracingAccelerationStructurePrebuildInfo(&as_inputs, &as_build_info);

    ComPtr<ID3D12Resource> scratch_buffer;
    ThrowIfFailedH(rt_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(
            as_build_info.ScratchDataSizeInBytes,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr,
        IID_PPV_ARGS(scratch_buffer.GetAddressOf())));
    
    ComPtr<ID3D12Resource> blas_res;
    ThrowIfFailedH(rt_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(
                as_build_info.ResultDataMaxSizeInBytes,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr,
            IID_PPV_ARGS(blas_res.GetAddressOf())));
    
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blas_desc = {};
    blas_desc.Inputs = as_inputs;
    blas_desc.DestAccelerationStructureData = blas_res->GetGPUVirtualAddress();
    blas_desc.ScratchAccelerationStructureData = scratch_buffer->GetGPUVirtualAddress();
    
    cmd_list.BuildRaytracingAccelerationStructure(&blas_desc, 0, nullptr);
    
    submesh.SetBLAS(std::move(blas_res));
}

void SceneManager::CalcSubmeshBoundingBox( StaticSubmesh_Deprecated& submesh )
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
