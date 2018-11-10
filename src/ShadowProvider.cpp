#include "stdafx.h"

#include "ShadowProvider.h"

using namespace DirectX;

// temporary implementation, only one shadow map 4kx4k max

ShadowProvider::ShadowProvider( ID3D12Device* device, int n_bufferized_frames, StagingDescriptorHeap* dsv_heap, DescriptorTableBakery* srv_tables, Scene* scene )
	: m_device( device ), m_scene( scene ), m_dsv_heap( dsv_heap ), m_descriptor_tables( srv_tables ), m_nbuffers( n_bufferized_frames )
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

	ThrowIfFailed( m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_UPLOAD ),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer( BufferGPUSize * m_nbuffers ),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS( m_pass_cb.GetAddressOf() ) ) );

	void* mapped_data = nullptr;
	ThrowIfFailed( m_pass_cb->Map( 0, nullptr, &mapped_data ) );

	m_mapped_data = span<uint8_t>( reinterpret_cast<uint8_t*>( mapped_data ),
								   reinterpret_cast<uint8_t*>( mapped_data ) + BufferGPUSize * m_nbuffers );
}


void ShadowProvider::Update( span<SceneLight> scene_lights, const Camera::Data& main_camera_data )
{
	m_producers.clear();

	bool shadow_found = false;
	for ( SceneLight& light : scene_lights )
	{
		if ( const auto& shadow_desc = light.GetShadow() )
		{
			if ( shadow_found )
				NOTIMPL;
			shadow_found = true;

			m_producers.emplace_back();
			auto& producer = m_producers.back();
			if ( shadow_desc->sm_size > ShadowMapSize )
				throw SnowEngineException( "shadow map is too big" );

			// viewport
			producer.map_data.viewport.MaxDepth = 1.0f;
			producer.map_data.viewport.MinDepth = 0.0f;
			producer.map_data.viewport.TopLeftX = 0.0f;
			producer.map_data.viewport.TopLeftY = 0.0f;
			producer.map_data.viewport.Width = shadow_desc->sm_size;
			producer.map_data.viewport.Height = shadow_desc->sm_size;

			// cb
			++m_cur_cb_idx %= m_nbuffers;
			PassConstants gpu_data;
			FillPassCB( light, shadow_desc.value(), main_camera_data.pos, gpu_data );
			const UINT offset_in_cb = BufferGPUSize * m_cur_cb_idx;
			memcpy( m_mapped_data.begin() + offset_in_cb, &gpu_data, BufferGPUSize );
			producer.map_data.pass_cb = m_pass_cb->GetGPUVirtualAddress() + offset_in_cb;
			CalcLightingPassShadowMatrix( light, gpu_data.ViewProj );
		}
	}
}


void ShadowProvider::FillPipelineStructures( span<const StaticMeshInstance> renderitems, ShadowProducers& producers, ShadowMapStorage& storage )
{
	// todo: frustrum cull renderitems

	for ( auto& producer : m_producers )
		producer.casters.clear();

	for ( const auto& mesh_instance : renderitems )
	{
		const StaticSubmesh& submesh = m_scene->AllStaticSubmeshes()[mesh_instance.Submesh()];
		const StaticMesh& geom = m_scene->AllStaticMeshes()[submesh.GetMesh()];
		if ( ! geom.IsLoaded() )
			continue;

		RenderItem item;
		item.ibv = geom.IndexBufferView();
		item.vbv = geom.VertexBufferView();
		item.index_count = submesh.DrawArgs().idx_cnt;
		item.index_offset = submesh.DrawArgs().start_index_loc;
		item.vertex_offset = submesh.DrawArgs().base_vertex_loc;

		const MaterialPBR& material = m_scene->AllMaterials()[mesh_instance.Material()];
		item.mat_cb = material.GPUConstantBuffer();
		item.mat_table = material.DescriptorTable();

		bool has_unloaded_texture = false;
		for ( TextureID tex_id : { material.Textures().base_color, material.Textures().normal, material.Textures().specular } )
		{
			if ( ! m_scene->AllTextures()[tex_id].IsLoaded() )
			{
				has_unloaded_texture = true;
				break;
			}
		}

		if ( has_unloaded_texture )
			continue;

		const ObjectTransform& tf = m_scene->AllTransforms()[mesh_instance.Transform()];
		item.tf_addr = tf.GPUView();

		for ( auto& producer : m_producers )
			producer.casters.push_back( item );
	}
	producers.arr = &m_producers;

	storage.res = m_sm_res.Get();
	storage.dsv = m_dsv->HandleCPU();
	storage.srv = m_descriptor_tables->GetTable( m_srv )->gpu_handle;
}


void ShadowProvider::FillPassCB( const SceneLight& light,
								 const SceneLight::Shadow& shadow_desc,
								 const DirectX::XMFLOAT3& camera_pos,
								 PassConstants& gpu_data )
{
	if ( light.GetData().type != SceneLight::LightType::Parallel )
		NOTIMPL;

	XMVECTOR dir = XMLoadFloat3( &light.GetData().dir );

	XMVECTOR target = XMLoadFloat3( &camera_pos );
	XMVECTOR pos = dir * shadow_desc.orthogonal_ws_height + target;
	XMVECTOR up = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );

	XMMATRIX view = XMMatrixLookAtLH( pos, target, up );
	XMMATRIX proj = XMMatrixOrthographicLH( shadow_desc.most_detailed_cascade_ws_halfwidth,
											shadow_desc.most_detailed_cascade_ws_halfwidth,
											shadow_desc.orthogonal_ws_height * 0.1f,
											shadow_desc.orthogonal_ws_height * 2.0f );
	const auto& viewproj = view * proj;
	XMStoreFloat4x4( &gpu_data.ViewProj, XMMatrixTranspose( viewproj ) );
	// we don't need other data, at least for now
}


void ShadowProvider::CalcLightingPassShadowMatrix( SceneLight& light, const DirectX::XMFLOAT4X4& pass_cb_matrix )
{
	// ToDo: calc corrected matrix in case of multiple lights
	light.ShadowMatrix() = XMMatrixTranspose( XMLoadFloat4x4( &pass_cb_matrix ) );
}
