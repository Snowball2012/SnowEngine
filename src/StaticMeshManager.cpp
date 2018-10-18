#include "stdafx.h"

#include "SceneManager.h"
#include "StaticMeshManager.h"


StaticMeshManager::StaticMeshManager( Microsoft::WRL::ComPtr<ID3D12Device> device, Scene* scene ) noexcept
	: m_device( std::move( device ) ), m_scene( scene )
{
}


void StaticMeshManager::LoadStaticMesh( StaticMeshID id, std::string name, const span<const Vertex>& vertices, const span<const uint32_t>& indices )
{
	NOTIMPL;
}


void StaticMeshManager::Update( SceneCopyOp operation_tag, GPUTaskQueue::Timestamp current_timestamp, ID3D12GraphicsCommandList& cmd_list )
{
	// Finalize existing transactions
	for ( auto tr_it = m_active_transactions.rbegin(); tr_it != m_active_transactions.rend(); )
	{
		auto& active_transaction = *tr_it;
		auto cur_it = tr_it++;
		if ( active_transaction.end_timestamp.has_value() && active_transaction.end_timestamp.value() < current_timestamp )
		{
			LoadMeshesFromTransaction( active_transaction.transaction );
			if ( cur_it != m_active_transactions.rbegin() )
				active_transaction = std::move( m_active_transactions.back() );

			m_active_transactions.pop_back();
		}
	}

	for ( auto mesh_it = m_loaded_meshes.begin(); mesh_it != m_loaded_meshes.end(); )
	{
		auto& mesh_data = *mesh_it;

		StaticMesh* mesh = m_scene->TryModifyStaticMesh( mesh_data.id );
		if ( ! mesh )
		{
			// remove mesh
			m_pending_transaction.meshes_to_remove.push_back( std::move( mesh_data ) );
			if ( mesh_it != std::prev( m_loaded_meshes.end() ) )
				mesh_data = std::move( m_loaded_meshes.back() );

			m_loaded_meshes.pop_back();
		}
		else
		{
			mesh_it++;
		}
	}

	if ( m_pending_transaction.meshes_to_upload.empty() && m_pending_transaction.meshes_to_remove.empty() )
		return; // nothing to do

	if ( m_pending_transaction.meshes_to_upload.size() != m_pending_transaction.meshes_to_upload.size() )
		throw SnowEngineException( "number of meshes must match the number of uploaders" );


	for ( size_t i = 0; i < m_pending_transaction.meshes_to_upload.size(); i++ )
		cmd_list.CopyResource( m_pending_transaction.meshes_to_upload[i].gpu_res.Get(), m_pending_transaction.uploaders[i].Get() );

	m_active_transactions.push_back( ActiveTransaction{ std::move( m_pending_transaction ), operation_tag, std::nullopt } );

	m_pending_transaction.meshes_to_remove.clear();
	m_pending_transaction.meshes_to_upload.clear();
	m_pending_transaction.uploaders.clear();
}


void StaticMeshManager::PostTimestamp( SceneCopyOp operation_tag, GPUTaskQueue::Timestamp end_timestamp )
{
	for ( auto& transaction : m_active_transactions )
		if ( transaction.operation_tag == operation_tag )
		{
			if ( transaction.end_timestamp.has_value() )
				throw SnowEngineException( "operation already has a timestamp" );
			else
				transaction.end_timestamp = end_timestamp;
		}
}


void StaticMeshManager::LoadEverythingBeforeTimestamp( GPUTaskQueue::Timestamp timestamp )
{
	for ( auto& active_transaction : m_active_transactions )
	{
		if ( active_transaction.end_timestamp.has_value() && active_transaction.end_timestamp.value() <= timestamp )
			LoadMeshesFromTransaction( active_transaction.transaction );
	}
}


void StaticMeshManager::LoadMeshesFromTransaction( UploadTransaction& transaction )
{
	m_loaded_meshes.reserve( m_loaded_meshes.size() + transaction.meshes_to_upload.size() );

	for ( auto& mesh_data : transaction.meshes_to_upload )
	{
		StaticMesh* mesh = m_scene->TryModifyStaticMesh( mesh_data.id );
		if ( mesh ) // mesh could have already been deleted
		{
			mesh->Load( mesh_data.vbv, mesh_data.ibv, mesh_data.topology );
			m_loaded_meshes.push_back( std::move( mesh_data ) );
		}
	}
	transaction.meshes_to_upload.clear();
}
