#include "stdafx.h"

#include "SceneManager.h"

#include "StaticMeshManager.h"

StaticMeshID SceneClientView::LoadStaticMesh( std::string name, const span<const Vertex>& vertices, const span<const uint32_t>& indices )
{
	StaticMeshID mesh_id = m_scene->AddStaticMesh();
	m_static_mesh_manager->LoadStaticMesh( mesh_id, std::move( name ), vertices, indices );
	return mesh_id;
}

SceneManager::SceneManager( Microsoft::WRL::ComPtr<ID3D12Device> device, size_t nframes_to_buffer, GPUTaskQueue* copy_queue )
	: m_static_mesh_mgr( std::move( device ), &m_scene )
	, m_scene_view( &m_scene, &m_static_mesh_mgr )
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

	// Start off in a closed state.  This is because the first time we refer 
	// to the command list we will Reset it, and it needs to be closed before
	// calling Reset.
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

	ID3D12CommandList* lists_to_exec[]{ m_cmd_list.Get() };
	m_copy_queue->GetCmdQueue()->ExecuteCommandLists( 1, lists_to_exec );
	
	m_static_mesh_mgr.PostTimestamp( cur_op, m_copy_queue->CreateTimestamp() );
}
