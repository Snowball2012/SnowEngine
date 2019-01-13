// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "ShadowProvider.h"

using namespace DirectX;

// temporary implementation, only one shadow map 4kx4k max

ShadowProvider::ShadowProvider( ID3D12Device* device, int n_bufferized_frames, StagingDescriptorHeap* dsv_heap, DescriptorTableBakery* srv_tables, Scene* scene )
	: m_device( device ), m_scene( scene ), m_dsv_heap( dsv_heap ), m_descriptor_tables( srv_tables )
{
	constexpr UINT width = ShadowMapSize;
	constexpr UINT height = ShadowMapSize;

	CD3DX12_RESOURCE_DESC tex_desc = CD3DX12_RESOURCE_DESC::Tex2D( ShadowMapResFormat,
																   width, height,
																   1, 1, 1, 0,
																   D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL );
	D3D12_CLEAR_VALUE opt_clear;
	opt_clear.Format = ShadowMapDSVFormat;
	opt_clear.DepthStencil.Depth = 1.0f;
	opt_clear.DepthStencil.Stencil = 0;

	{
		ThrowIfFailed( m_device->CreateCommittedResource( &CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT ), D3D12_HEAP_FLAG_NONE,
														  &tex_desc, D3D12_RESOURCE_STATE_COMMON,
														  &opt_clear, IID_PPV_ARGS( &m_sm_res ) ) );

		m_srv = m_descriptor_tables->AllocateTable( 1 );
		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
		{
			srv_desc.Format = ShadowMapSRVFormat;
			srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MipLevels = 1;
		}
		m_device->CreateShaderResourceView( m_sm_res.Get(), &srv_desc, *m_descriptor_tables->ModifyTable( m_srv ) );

		m_dsv = std::make_unique<Descriptor>( std::move( m_dsv_heap->AllocateDescriptor() ) );
		D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
		{
			dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsv_desc.Format = ShadowMapDSVFormat;
			dsv_desc.Texture2D.MipSlice = 0;
			dsv_desc.Flags = D3D12_DSV_FLAG_NONE;
		}
		m_device->CreateDepthStencilView( m_sm_res.Get(), &dsv_desc, m_dsv->HandleCPU() );
	}

	{
		tex_desc.DepthOrArraySize = MAX_CASCADE_SIZE;
		ThrowIfFailed( m_device->CreateCommittedResource( &CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT ), D3D12_HEAP_FLAG_NONE,
														  &tex_desc, D3D12_RESOURCE_STATE_COMMON,
														  &opt_clear, IID_PPV_ARGS( &m_pssm_res ) ) );

		m_pssm_srv = m_descriptor_tables->AllocateTable( 1 );
		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
		{
			srv_desc.Format = ShadowMapSRVFormat;
			srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srv_desc.Texture2DArray.MipLevels = 1;
			srv_desc.Texture2DArray.ArraySize = MAX_CASCADE_SIZE;
		}
		m_device->CreateShaderResourceView( m_pssm_res.Get(), &srv_desc, *m_descriptor_tables->ModifyTable( m_pssm_srv ) );

		m_pssm_dsv = std::make_unique<Descriptor>( std::move( m_dsv_heap->AllocateDescriptor() ) );
		D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
		{
			dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			dsv_desc.Format = ShadowMapDSVFormat;
			dsv_desc.Texture2DArray.ArraySize = MAX_CASCADE_SIZE;
			dsv_desc.Flags = D3D12_DSV_FLAG_NONE;
		}
		m_device->CreateDepthStencilView( m_pssm_res.Get(), &dsv_desc, m_pssm_dsv->HandleCPU() );
	}
}


void ShadowProvider::Update( span<SceneLight> scene_lights, const ParallelSplitShadowMapping& pssm, const Camera::Data& main_camera_data )
{
	std::array<float, MAX_CASCADE_SIZE - 1> split_positions;

	pssm.CalcSplitPositionsVS( main_camera_data, make_span( split_positions.data(), split_positions.data() + split_positions.size() ) );

	for ( SceneLight& light : scene_lights )
	{
		if ( ! light.IsEnabled() )
			continue;

		if ( const auto& shadow_desc = light.GetShadow() )
		{
			const bool use_csm = shadow_desc->num_cascades > 1
				                 || light.GetData().type == SceneLight::LightType::Parallel; // right now shaders require all parallel lights to use pssm

			if ( light.GetData().type != SceneLight::LightType::Parallel && use_csm )
				NOTIMPL;

			if ( shadow_desc->num_cascades > MAX_CASCADE_SIZE )
				throw SnowEngineException( "too many cascades for a light" );

			auto& shadow_matrices = light.SetShadowMatrices();
			shadow_matrices.resize( shadow_desc->num_cascades );

			if ( use_csm )
			{
				for ( int i = 0; i < shadow_matrices.size(); ++i )
				{
					DirectX::XMFLOAT3 pos = main_camera_data.dir;
					if ( i > 0 )
						pos *= split_positions[i - 1];
					pos += main_camera_data.pos;
					shadow_matrices[i] = CalcShadowMatrix( light, pos, shadow_desc.value() );
				}
			}
		}
	}
}


void ShadowProvider::FillPipelineStructures( const span<const LightInCB>& lights, const span<const StaticMeshInstance>& renderitems, ShadowProducers& producers, ShadowCascadeProducers& pssm_producers, ShadowMapStorage& storage, ShadowCascadeStorage& pssm_storage )
{
	// todo: frustrum cull renderitems
	CreateShadowProducers( lights );
	FillProducersWithRenderitems( renderitems );

	producers.arr = make_span( m_producers.data(), m_producers.data() + m_producers.size() );

	storage.res = m_sm_res.Get();
	storage.dsv = m_dsv->HandleCPU();
	storage.srv = m_descriptor_tables->GetTable( m_srv )->gpu_handle;

	pssm_producers.arr = make_span( m_pssm_producers.data(), m_pssm_producers.data() + m_pssm_producers.size() );

	pssm_storage.res = m_pssm_res.Get();
	pssm_storage.dsv = m_pssm_dsv->HandleCPU();
	pssm_storage.srv = m_descriptor_tables->GetTable( m_pssm_srv )->gpu_handle;
}


XMMATRIX ShadowProvider::CalcShadowMatrix( const SceneLight& light, const XMFLOAT3& camera_pos,
										   const SceneLight::Shadow& shadow_desc ) const
{
	if ( light.GetData().type != SceneLight::LightType::Parallel )
		NOTIMPL;

	XMVECTOR dir = XMLoadFloat3( &light.GetData().dir );

	XMVECTOR target = XMLoadFloat3( &camera_pos );
	XMVECTOR pos = dir * shadow_desc.orthogonal_ws_height + target;
	XMVECTOR up = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );

	XMMATRIX view = XMMatrixLookAtLH( pos, target, up );
	XMMATRIX proj = XMMatrixOrthographicLH( shadow_desc.ws_halfwidth,
											shadow_desc.ws_halfwidth,
											shadow_desc.orthogonal_ws_height * 0.1f,
											shadow_desc.orthogonal_ws_height * 2.0f );

	return view * proj;
}


void ShadowProvider::CreateShadowProducers( const span<const LightInCB>& lights )
{
	m_pssm_producers.clear();
	m_producers.clear();

	bool pssm_light_with_shadow_found = false;
	bool regular_light_with_shadow_found = false;
	for ( const LightInCB& light_in_cb : lights )
	{
		const SceneLight& light = *light_in_cb.light;

		const auto& shadow_desc = light.GetShadow();
		if ( ! shadow_desc )
			continue;

		if ( light.GetShadowMatrices().size() > 1 || light.GetData().type == SceneLight::LightType::Parallel )
		{
			if ( pssm_light_with_shadow_found )
				NOTIMPL; // pack all pssm shadow maps in one

			if ( light.GetData().type != SceneLight::LightType::Parallel )
				NOTIMPL; // only parallel lights may be pssm

			pssm_light_with_shadow_found = true;

			m_pssm_producers.emplace_back();
			auto& producer = m_pssm_producers.back();
			if ( light.GetShadow()->sm_size > PSSMShadowMapSize )
				throw SnowEngineException( "pssm shadow map does not fit in texture" );

			// viewport
			producer.viewport.MaxDepth = 1.0f;
			producer.viewport.MinDepth = 0.0f;
			producer.viewport.TopLeftX = 0.0f;
			producer.viewport.TopLeftY = 0.0f;
			producer.viewport.Width = shadow_desc->sm_size;
			producer.viewport.Height = shadow_desc->sm_size;

			producer.light_idx_in_cb = light_in_cb.light_idx_in_cb;
		}
		else if ( light.GetShadowMatrices().size() == 1 )
		{
			NOTIMPL; // TODO: restore regular shadow mapping
		}
	}
}


void ShadowProvider::FillProducersWithRenderitems( const span<const StaticMeshInstance>& renderitems )
{
	for ( const auto& mesh_instance : renderitems )
	{
		if ( ! mesh_instance.IsEnabled() )
			continue;

		const StaticSubmesh& submesh = m_scene->AllStaticSubmeshes()[mesh_instance.Submesh()];
		const StaticMesh& geom = m_scene->AllStaticMeshes()[submesh.GetMesh()];
		if ( ! geom.IsLoaded() )
			continue;

		RenderItem item;
		item.ibv = geom.IndexBufferView();
		item.vbv = geom.VertexBufferView();

		const auto& submesh_draw_args = submesh.DrawArgs();
		item.index_count = submesh_draw_args.idx_cnt;
		item.index_offset = submesh_draw_args.start_index_loc;
		item.vertex_offset = submesh_draw_args.base_vertex_loc;

		const MaterialPBR& material = m_scene->AllMaterials()[mesh_instance.Material()];
		item.mat_cb = material.GPUConstantBuffer();
		item.mat_table = material.DescriptorTable();

		bool has_unloaded_texture = false;
		const auto& textures = material.Textures();
		for ( TextureID tex_id : { textures.base_color, textures.normal, textures.specular } )
		{
			if ( ! m_scene->AllTextures()[tex_id].IsLoaded() )
			{
				has_unloaded_texture = true;
				break;
			}
		}

		if ( has_unloaded_texture )
			continue;

		const ObjectTransform& tf = m_scene->AllTransforms()[mesh_instance.GetTransform()];
		item.tf_addr = tf.GPUView();

		for ( auto& producer : m_pssm_producers )
			producer.casters.push_back( item );

		for ( auto& producer : m_producers )
			producer.casters.push_back( item );
	}
}
