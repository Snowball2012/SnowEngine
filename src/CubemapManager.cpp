// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "CubemapManager.h"
#include "SceneManager.h"
#include "CubemapConverter.h"

#include <dxtk12/DDSTextureLoader.h>
#include <dxtk12/DirectXHelpers.h>

CubemapManager::CubemapManager( Microsoft::WRL::ComPtr<ID3D12Device> device, DescriptorTableBakery* descriptor_tables, Scene* scene ) noexcept
	: m_device( std::move( device ) ), m_scene( scene )
	, m_srv_heap( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_device )
	, m_desc_tables( descriptor_tables )
	, m_converter( m_device )
{
}

CubemapManager::~CubemapManager()
{
}


void CubemapManager::CreateCubemapFromTexture( CubemapID cubemap_id, TextureID texture_id )
{
	assert( m_scene->AllTextures().has( texture_id ) );
	assert( m_scene->AllCubemaps().has( cubemap_id ) );

	m_conversion_in_progress.emplace_back();
	auto& new_cubemap = m_conversion_in_progress.back();
	new_cubemap.texture_id = texture_id;

	m_scene->TryModifyTexture( texture_id )->AddRef();

	new_cubemap.texture_srv = DescriptorTableBakery::TableID::nullid;
	new_cubemap.cubemap.id = cubemap_id;
}


void CubemapManager::Update( )
{
	// Todo: remove missing cubemaps

	for ( auto& cubemap : m_conversion_in_progress )
	{
		const auto& texture = m_scene->AllTextures()[cubemap.texture_id];
		if ( texture.IsLoaded() )
		{
			cubemap.texture_srv = m_desc_tables->AllocateTable( 1 );
			m_device->CopyDescriptorsSimple( 1, m_desc_tables->ModifyTable( cubemap.texture_srv ).value(),
											 texture.StagingSRV(),
											 D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
		}
	}
}


void CubemapManager::OnBakeDescriptors( ID3D12GraphicsCommandList& cmd_list_graphics_queue )
{
	for ( auto i = m_conversion_in_progress.begin(); i != m_conversion_in_progress.end(); )
	{
		if ( ! ( i->texture_srv == DescriptorTableBakery::TableID::nullid ) )
		{
			i->cubemap.gpu_res = m_converter.MakeCubemapFromCylindrical( CubemapResolution,
																			  m_desc_tables->GetTable( i->texture_srv )->gpu_handle,
																			  DXGI_FORMAT_R16G16B16A16_FLOAT, cmd_list_graphics_queue );

			cmd_list_graphics_queue.ResourceBarrier( 1,
													 &CD3DX12_RESOURCE_BARRIER::Transition( i->cubemap.gpu_res.Get(),
																							D3D12_RESOURCE_STATE_RENDER_TARGET,
																							D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE ) );

			if ( ! i->cubemap.gpu_res )
				throw SnowEngineException( "convertation failed" );

			// TODO: finish
			i->cubemap.staging_srv = std::make_unique<Descriptor>( std::move( m_srv_heap.AllocateDescriptor() ) );
			DirectX::CreateShaderResourceView( m_device.Get(), i->cubemap.gpu_res.Get(), i->cubemap.staging_srv->HandleCPU(), true );

			m_scene->TryModifyCubemap( i->cubemap.id )->Load( i->cubemap.staging_srv->HandleCPU() );
			m_loaded_cubemaps.emplace_back( std::move( i->cubemap ) );
			i = m_conversion_in_progress.erase( i ); // TODO: change to swap-erase, check for cubemap existance
		}
		else
		{
			i++;
		}
	}
}
