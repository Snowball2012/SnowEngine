#include "stdafx.h"

#include "SceneManager.h"

#include "StaticMeshManager.h"

StaticMeshID SceneClientView::LoadStaticMesh( std::string name, const span<const Vertex>& vertices, const span<const uint32_t>& indices )
{
	StaticMeshID mesh_id = m_scene->AddStaticMesh();
	m_static_mesh_manager->LoadStaticMesh( mesh_id, std::move( name ), vertices, indices );
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
	return mat_id;
}

SceneManager::SceneManager( Microsoft::WRL::ComPtr<ID3D12Device> device, size_t nframes_to_buffer, GPUTaskQueue* copy_queue )
	: m_static_mesh_mgr( device, &m_scene )
	, m_tex_streamer( device, &m_scene )
	, m_dynamic_buffers( device, &m_scene, nframes_to_buffer )
	, m_scene_view( &m_scene, &m_static_mesh_mgr, &m_tex_streamer, &m_dynamic_buffers )
	, m_copy_queue( copy_queue )
	, m_nframes_to_buffer( nframes_to_buffer )
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


void SceneManager::UpdatePipelineBindings()
{
	GPUTaskQueue::Timestamp current_copy_time = m_copy_queue->GetCurrentTimestamp();

	SceneCopyOp cur_op = m_operation_counter++;

	m_cmd_list->Reset( m_cmd_allocators[cur_op % m_nframes_to_buffer].Get(), nullptr );
	m_static_mesh_mgr.Update( cur_op, current_copy_time, *m_cmd_list.Get() );
	m_tex_streamer.Update( cur_op, current_copy_time, *m_cmd_list.Get() );
	m_dynamic_buffers.Update();

	ThrowIfFailed( m_cmd_list->Close() );
	ID3D12CommandList* lists_to_exec[]{ m_cmd_list.Get() };
	m_copy_queue->GetCmdQueue()->ExecuteCommandLists( 1, lists_to_exec );
	
	auto timestamp = m_copy_queue->CreateTimestamp();
	m_static_mesh_mgr.PostTimestamp( cur_op, timestamp );
	m_tex_streamer.PostTimestamp( cur_op, timestamp );

	CleanModifiedItemsStatus();
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
