// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com
#include "stdafx.h"

#include "SceneManager.h"
#include "StaticMeshManager.h"


StaticMeshManager::StaticMeshManager( Microsoft::WRL::ComPtr<ID3D12Device> device, Scene* scene ) noexcept
	: m_device( std::move( device ) ), m_scene( scene )
{
}

namespace
{
	void CreateBuffersAndInitializeUploader( ID3D12Device& device,
											 Microsoft::WRL::ComPtr<ID3D12Resource>& gpu_res,
											 Microsoft::WRL::ComPtr<ID3D12Resource>& uploader,
											 const void* data, size_t size_in_bytes )
	{
		ThrowIfFailed( device.CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT ),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer( size_in_bytes ),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS( gpu_res.GetAddressOf() ) ) );

		ThrowIfFailed( device.CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_UPLOAD ),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer( size_in_bytes ),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS( uploader.GetAddressOf() ) ) );

		uint8_t* mapped_uploader;
		ThrowIfFailed( uploader->Map( 0, nullptr, reinterpret_cast<void**>( &mapped_uploader ) ) );

		memcpy( mapped_uploader, data, size_in_bytes );

		uploader->Unmap( 0, nullptr );
	}
}

void StaticMeshManager::LoadStaticMesh( StaticMeshID id, std::string name, const span<const Vertex>& vertices, const span<const uint32_t>& indices )
{
	m_pending_transaction.meshes_to_upload.emplace_back();
	auto& mesh_data = m_pending_transaction.meshes_to_upload.back();
	m_pending_transaction.uploaders.emplace_back();
	auto& uploaders = m_pending_transaction.uploaders.back();

	mesh_data.id = id;
	mesh_data.name = std::move( name );
	mesh_data.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	const size_t vb_byte_size = vertices.size() * sizeof( Vertex );
	const size_t ib_byte_size = indices.size() * sizeof( uint32_t );

	CreateBuffersAndInitializeUploader( *m_device.Get(), mesh_data.gpu_vb, uploaders.first, vertices.cbegin(), vb_byte_size );
	mesh_data.vbv.BufferLocation = mesh_data.gpu_vb->GetGPUVirtualAddress();
	mesh_data.vbv.SizeInBytes = UINT( vb_byte_size );
	mesh_data.vbv.StrideInBytes = sizeof( Vertex );

	CreateBuffersAndInitializeUploader( *m_device.Get(), mesh_data.gpu_ib, uploaders.second, indices.cbegin(), ib_byte_size );
	mesh_data.ibv.BufferLocation = mesh_data.gpu_ib->GetGPUVirtualAddress();
	mesh_data.ibv.Format = DXGI_FORMAT_R32_UINT;
	mesh_data.ibv.SizeInBytes = UINT( ib_byte_size );
}


void StaticMeshManager::Update( SceneCopyOp operation_tag, GPUTaskQueue::Timestamp current_timestamp, ID3D12GraphicsCommandList& cmd_list )
{
	// Finalize existing transactions
	for ( size_t i = 0; i < m_active_transactions.size(); )
	{
		auto& active_transaction = m_active_transactions[i];

		if ( active_transaction.end_timestamp.has_value()
			 && active_transaction.end_timestamp.value() <= current_timestamp )
		{
			LoadMeshesFromTransaction( active_transaction.transaction );
			if ( i != m_active_transactions.size() - 1 )
				active_transaction = std::move( m_active_transactions.back() );

			m_active_transactions.pop_back();
		}
		else
		{
			i++;
		}
	}

	for ( size_t i = 0; i < m_loaded_meshes.size(); )
	{
		auto& mesh_data = m_loaded_meshes[i];

		StaticMesh* mesh = m_scene->TryModifyStaticMesh( mesh_data.id );
		if ( ! mesh )
		{
			// remove mesh
			m_pending_transaction.meshes_to_remove.push_back( std::move( mesh_data ) );
			if ( i != m_loaded_meshes.size() - 1 )
				mesh_data = std::move( m_loaded_meshes.back() );

			m_loaded_meshes.pop_back();
		}
		else
		{
			i++;
		}
	}

	if ( m_pending_transaction.meshes_to_upload.empty() && m_pending_transaction.meshes_to_remove.empty() )
		return; // nothing to do

	if ( m_pending_transaction.meshes_to_upload.size() != m_pending_transaction.meshes_to_upload.size() )
		throw SnowEngineException( "number of meshes must match the number of uploaders" );

	for ( size_t i = 0; i < m_pending_transaction.meshes_to_upload.size(); i++ )
	{
		cmd_list.CopyResource( m_pending_transaction.meshes_to_upload[i].gpu_vb.Get(), m_pending_transaction.uploaders[i].first.Get() );
		cmd_list.CopyResource( m_pending_transaction.meshes_to_upload[i].gpu_ib.Get(), m_pending_transaction.uploaders[i].second.Get() );
	}

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
			mesh->Load( mesh_data.vbv, mesh_data.ibv );
			m_loaded_meshes.push_back( std::move( mesh_data ) );
		}
	}
	transaction.meshes_to_upload.clear();
}
