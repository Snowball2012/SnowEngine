#include "stdafx.h"

#include "SceneManager.h"

#include "StaticMeshManager.h"

#include <DirectXMath.h>

StaticMeshID SceneClientView::LoadStaticMesh( std::string name, std::vector<Vertex> vertices, std::vector<uint32_t> indices )
{
	StaticMeshID mesh_id = m_scene->AddStaticMesh();
	m_static_mesh_manager->LoadStaticMesh( mesh_id, std::move( name ),
										   make_span( vertices.data(), vertices.data() + vertices.size() ),
										   make_span( indices.data(), indices.data() + indices.size() ) );
	m_scene->TryModifyStaticMesh( mesh_id )->Vertices() = std::move( vertices );
	m_scene->TryModifyStaticMesh( mesh_id )->Indices() = std::move( indices );
	return mesh_id;
}

TextureID SceneClientView::LoadStreamedTexture( std::string path )
{
	TextureID tex_id = m_scene->AddTexture();
	m_tex_streamer->LoadStreamedTexture( tex_id, std::move( path ) );
	return tex_id;
}

TransformID SceneClientView::AddTransform( DirectX::XMFLOAT4X4 obj2world )
{
	TransformID tf_id = m_scene->AddTransform();
	m_scene->TryModifyTransform( tf_id )->ModifyMat() = obj2world;

	m_dynamic_buffers->AddTransform( tf_id );
	return tf_id;
}

MaterialID SceneClientView::AddMaterial( const MaterialPBR::TextureIds& textures, DirectX::XMFLOAT3 diffuse_fresnel, DirectX::XMFLOAT4X4 uv_transform )
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



SceneManager::SceneManager( Microsoft::WRL::ComPtr<ID3D12Device> device, StagingDescriptorHeap* dsv_heap, size_t nframes_to_buffer, GPUTaskQueue* copy_queue )
	: m_static_mesh_mgr( device, &m_scene )
	, m_tex_streamer( device, &m_scene )
	, m_dynamic_buffers( device, &m_scene, nframes_to_buffer )
	, m_scene_view( &m_scene, &m_static_mesh_mgr, &m_tex_streamer, &m_dynamic_buffers, &m_material_table_baker )
	, m_copy_queue( copy_queue )
	, m_nframes_to_buffer( nframes_to_buffer )
	, m_gpu_descriptor_tables( device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, nframes_to_buffer )
	, m_material_table_baker( device, &m_scene, &m_gpu_descriptor_tables )
	, m_shadow_provider( device.Get(), int( nframes_to_buffer ), dsv_heap, &m_gpu_descriptor_tables, &m_scene )
{
	for ( size_t i = 0; i < m_nframes_to_buffer; ++i )
	{
		m_cmd_allocators.emplace_back();
		ThrowIfFailed( device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_COPY,
			IID_PPV_ARGS( m_cmd_allocators.back().GetAddressOf() ) ) );
	}

	ThrowIfFailed( device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_COPY,
		m_cmd_allocators[0].Get(), // Associated command allocator
		nullptr,                   // Initial PipelineStateObject
		IID_PPV_ARGS( m_cmd_list.GetAddressOf() ) ) );

	// Later we will call Reset on cmd_list, which demands for the list to be closed
	m_cmd_list->Close();
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

void SceneManager::UpdatePipelineBindings( CameraID main_camera_id )
{
	m_main_camera_id = main_camera_id;

	GPUTaskQueue::Timestamp current_copy_time = m_copy_queue->GetCurrentTimestamp();

	SceneCopyOp cur_op = m_operation_counter++;
	ThrowIfFailed( m_cmd_allocators[cur_op % m_nframes_to_buffer]->Reset() );
	m_cmd_list->Reset( m_cmd_allocators[cur_op % m_nframes_to_buffer].Get(), nullptr );

	m_static_mesh_mgr.Update( cur_op, current_copy_time, *m_cmd_list.Get() );
	ProcessSubmeshes();
	m_tex_streamer.Update( cur_op, current_copy_time, *m_cmd_list.Get() );
	m_dynamic_buffers.Update();
	m_material_table_baker.UpdateStagingDescriptors();
	if ( const Camera* main_cam = m_scene.AllCameras().try_get( m_main_camera_id ) )
		m_shadow_provider.Update( m_scene.LightSpan(), main_cam->GetData() );

	ThrowIfFailed( m_cmd_list->Close() );
	ID3D12CommandList* lists_to_exec[]{ m_cmd_list.Get() };
	m_copy_queue->GetCmdQueue()->ExecuteCommandLists( 1, lists_to_exec );
	
	m_last_copy_timestamp = m_copy_queue->CreateTimestamp();
	m_static_mesh_mgr.PostTimestamp( cur_op, m_last_copy_timestamp );
	m_tex_streamer.PostTimestamp( cur_op, m_last_copy_timestamp );

	if ( m_gpu_descriptor_tables.BakeGPUTables() )
		m_material_table_baker.UpdateGPUDescriptors();

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
