// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "SceneRenderer.h"

#include "FramegraphResource.h"
#include "Framegraph.h"


SceneRenderer SceneRenderer::Create( const DeviceContext& ctx, uint32_t width, uint32_t height, uint32_t n_frames_in_flight )
{
	NOTIMPL;
}


SceneRenderer::SceneRenderer( const DeviceContext& ctx,
							  uint32_t width, uint32_t height,
							  uint32_t n_frames_in_flight,
							  StagingDescriptorHeap&& dsv_heap,
							  StagingDescriptorHeap&& rtv_heap,
							  ForwardCBProvider&& forward_cb_provider,
							  ShadowProvider&& shadow_provider ) noexcept
	: m_dsv_heap( std::move( dsv_heap ) )
	, m_rtv_heap( std::move( rtv_heap ) )
	, m_forward_cb_provider( std::move( forward_cb_provider ) )
	, m_shadow_provider( std::move( shadow_provider ) )
	, m_n_frames_in_flight( n_frames_in_flight )
{
	assert( ctx.device );
	assert( ctx.srv_cbv_uav_tables );
	assert( n_frames_in_flight > 0 );
	assert( width > 0 );
	assert( height > 0 );

	m_device = ctx.device;
	m_descriptor_tables = ctx.srv_cbv_uav_tables;

	m_resolution_width = width;
	m_resolution_height = height;
	CreateTransientResources();

	InitFramegraph();
}


void SceneRenderer::InitFramegraph()
{
	m_framegraph.ConstructAndEnableNode<DepthPrepassNode>( m_depth_stencil_format, m_device );

	m_framegraph.ConstructAndEnableNode<ShadowPassNode>( m_depth_stencil_format, m_shadow_bias, m_device );
	m_framegraph.ConstructAndEnableNode<PSSMNode>( m_depth_stencil_format, m_shadow_bias, m_device );

	m_framegraph.ConstructAndEnableNode<ForwardPassNode>( m_hdr_format, // rendertarget
														  m_hdr_format, // ambient lighting
														  m_normals_format, // normals
														  m_depth_stencil_format, m_device );

	m_framegraph.ConstructAndEnableNode<SkyboxNode>( m_hdr_format, m_depth_stencil_format, m_device );

	m_framegraph.ConstructAndEnableNode<HBAONode>( m_ssao->Resource()->GetDesc().Format, m_device );
	m_framegraph.ConstructAndEnableNode<BlurSSAONode>( m_device );
	m_framegraph.ConstructAndEnableNode<ToneMapNode>( m_back_buffer_format, m_device );

	if ( m_framegraph.IsRebuildNeeded() )
		m_framegraph.Rebuild();
}


void SceneRenderer::CreateTransientResources()
{
	// little hack here, create 1x1 textures and resize them right after

	auto create_tex = [&]( std::unique_ptr<DynamicTexture>& tex, DXGI_FORMAT texture_format,
						   bool create_srv_table, bool create_uav_table, bool create_rtv_desc )
	{
		CD3DX12_RESOURCE_DESC desc( CD3DX12_RESOURCE_DESC::Tex2D( texture_format,
																  /*width=*/1, /*height=*/1,
																  /*depth=*/1, /*mip_levels=*/1 ) );

		D3D12_CLEAR_VALUE clear_value;
		clear_value.Color[0] = 0;
		clear_value.Color[1] = 0;
		clear_value.Color[2] = 0;
		clear_value.Color[3] = 0;
		clear_value.Format = texture_format;

		if ( create_uav_table )
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		if ( create_rtv_desc )
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		// create resource (maybe create it in DynamicTexture?)
		{
			const D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON;

			const auto* opt_clear_ptr = create_rtv_desc ? &clear_value : nullptr;
			ComPtr<ID3D12Resource> res;
			ThrowIfFailed( m_device->CreateCommittedResource( &CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT ), D3D12_HEAP_FLAG_NONE,
															  &desc, initial_state,
															  opt_clear_ptr,
															  IID_PPV_ARGS( res.GetAddressOf() ) ) );

			tex = std::make_unique<DynamicTexture>( std::move( res ), m_device, initial_state, opt_clear_ptr );
		}

		if ( create_srv_table )
			tex->SRV() = m_descriptor_tables->AllocateTable( 1 );
		if ( create_uav_table )
			tex->UAV() = m_descriptor_tables->AllocateTable( 1 );
		if ( create_rtv_desc )
			tex->RTV() = std::make_unique<Descriptor>( std::move( m_rtv_heap.AllocateDescriptor() ) );
	};

	create_tex( m_hdr_backbuffer, m_hdr_format, true, false, true );
	create_tex( m_hdr_ambient, m_hdr_format, true, false, true );
	create_tex( m_normals, m_normals_format, true, false, true );
	create_tex( m_ssao, m_ssao_format, true, false, true );
	create_tex( m_ssao_blurred, m_ssao_format, true, true, false );
	create_tex( m_ssao_blurred_transposed, m_ssao_format, true, true, false );

	NOTIMPL; // depth_stencil buffer
	m_depth_buffer_srv = DescriptorTables().AllocateTable( 1 );

	ResizeTransientResources();
}


void SceneRenderer::ResizeTransientResources()
{
	auto resize_tex = [&]( auto& tex, uint32_t width, uint32_t height, bool make_srv, bool make_uav, bool make_rtv )
	{
		tex.Resize( width, height );
		if ( make_srv )
			m_d3d_device->CreateShaderResourceView( tex.Resource(), nullptr, *DescriptorTables().ModifyTable( tex.SRV() ) );
		if ( make_uav )
			m_d3d_device->CreateUnorderedAccessView( tex.Resource(), nullptr, nullptr, *DescriptorTables().ModifyTable( tex.UAV() ) );
		if ( make_rtv )
			m_d3d_device->CreateRenderTargetView( tex.Resource(), nullptr, tex.RTV()->HandleCPU() );
	};

	resize_tex( *m_fp_backbuffer, m_client_width, m_client_height, true, false, true );
	resize_tex( *m_ambient_lighting, m_client_width, m_client_height, true, false, true );
	resize_tex( *m_normals, m_client_width, m_client_height, true, false, true );
	resize_tex( *m_ssao, m_client_width / 2, m_client_height / 2, true, false, true );
	resize_tex( *m_ssao_blurred, m_client_width, m_client_height, true, true, false );
	resize_tex( *m_ssao_blurred_transposed, m_client_height, m_client_width, true, true, false );

	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
	{
		srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = 1;
	}
	m_d3d_device->CreateShaderResourceView( m_depth_stencil_buffer.Get(), &srv_desc, DescriptorTables().ModifyTable( m_depth_buffer_srv ).value() );

}


void SceneRenderer::Draw( const SceneContext& scene_ctx, const FrameContext& frame_ctx, RenderMode mode,
						  std::vector<CommandList> graphics_cmd_lists )
{
	assert( scene_ctx.main_camera != CameraID::nullid );
	assert( scene_ctx.scene != nullptr );

	if ( mode != RenderMode::FullTonemapped )
		NOTIMPL;

	m_framegraph.ClearResources();

	if ( m_framegraph.IsRebuildNeeded() )
		m_framegraph.Rebuild();

	Scene& scene = *scene_ctx.scene;

	const Camera* main_camera = scene.AllCameras().try_get( scene_ctx.main_camera );
	if ( ! main_camera )
		throw SnowEngineException( "no main camera" );

	std::vector<RenderItem> lighting_items = CreateRenderitems( main_camera->GetData(), scene );

	m_shadow_provider.Update( scene.LightSpan(), m_pssm, main_camera->GetData() );
	m_forward_cb_provider.Update( main_camera->GetData(), m_pssm, scene.LightSpan() );

	{
		ShadowProducers producers;
		ShadowMaps sm_storage;
		ShadowCascadeProducers pssm_producers;
		ShadowCascade pssm_storage;
		m_shadow_provider.FillFramegraphStructures( m_forward_cb_provider.GetLightsInCB(), scene.StaticMeshInstanceSpan(), producers, pssm_producers, sm_storage, pssm_storage );
		m_framegraph.SetRes( producers );
		m_framegraph.SetRes( sm_storage );
		m_framegraph.SetRes( pssm_producers );
		m_framegraph.SetRes( pssm_storage );
	}

	{
		MainRenderitems forward_renderitems;
		forward_renderitems.items = make_span( lighting_items );
		m_framegraph.SetRes( forward_renderitems );
	}

	// Reuse memory associated with command recording
	// We can only reset when the associated command lists have finished execution on GPU
	CommandList cmd_list = frame_ctx.cmd_list_pool->GetList( D3D12_COMMAND_LIST_TYPE_DIRECT );
	cmd_list.Reset();

	ID3D12GraphicsCommandList* list_iface = cmd_list.GetInterface();

	constexpr uint32_t nbarriers = 8;
	CD3DX12_RESOURCE_BARRIER rtv_barriers[nbarriers];
	rtv_barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition( m_hdr_backbuffer->Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET );
	rtv_barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition( m_hdr_ambient->Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET );
	rtv_barriers[2] = CD3DX12_RESOURCE_BARRIER::Transition( m_normals->Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET );
	rtv_barriers[3] = CD3DX12_RESOURCE_BARRIER::Transition( m_ssao->Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET );
	rtv_barriers[4] = CD3DX12_RESOURCE_BARRIER::Transition( m_ssao_blurred->Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
	rtv_barriers[5] = CD3DX12_RESOURCE_BARRIER::Transition( m_ssao_blurred_transposed->Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
	
	{
		auto& sm_storage = m_framegraph.GetRes<ShadowMaps>();
		if ( ! sm_storage )
			throw SnowEngineException( "missing resource" );
		rtv_barriers[6] = CD3DX12_RESOURCE_BARRIER::Transition( sm_storage->res, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE );
		auto& pssm_storage = m_framegraph.GetRes<ShadowCascade>();
		if ( ! pssm_storage )
			throw SnowEngineException( "missing resource" );
		rtv_barriers[7] = CD3DX12_RESOURCE_BARRIER::Transition( pssm_storage->res, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE );
	}

	list_iface->ResourceBarrier( nbarriers, rtv_barriers );
	ID3D12DescriptorHeap* heaps[] = { m_descriptor_tables->CurrentGPUHeap().Get() };
	list_iface->SetDescriptorHeaps( 1, heaps );

	{
		m_framegraph.SetRes( CreateSkybox( main_camera->GetSkybox(), scene_ctx.ibl_table, scene ) );

		ForwardPassCB pass_cb{ m_forward_cb_provider.GetCBPointer() };
		m_framegraph.SetRes( pass_cb );

		HDRBuffer hdr_buffer;
		hdr_buffer.res = m_hdr_backbuffer->Resource();
		hdr_buffer.rtv = m_hdr_backbuffer->RTV()->HandleCPU();
		hdr_buffer.srv = GetGPUHandle( m_hdr_backbuffer->SRV() );
		m_framegraph.SetRes( hdr_buffer );

		AmbientBuffer ambient_buffer;
		ambient_buffer.res = m_hdr_ambient->Resource();
		ambient_buffer.rtv = m_hdr_ambient->RTV()->HandleCPU();
		ambient_buffer.srv = GetGPUHandle( m_hdr_ambient->SRV() );
		m_framegraph.SetRes( ambient_buffer );

		NormalBuffer normal_buffer;
		normal_buffer.res = m_normals->Resource();
		normal_buffer.rtv = m_normals->RTV()->HandleCPU();
		normal_buffer.srv = GetGPUHandle( m_normals->SRV() );
		m_framegraph.SetRes( normal_buffer );

		DepthStencilBuffer depth_buffer;
		depth_buffer.dsv = m_depth_stencil_buffer->DSV()->HandleCPU();
		depth_buffer.srv = GetGPUHandle( m_depth_stencil_buffer->SRV() );
		m_framegraph.SetRes( depth_buffer );

		ScreenConstants screen;
		screen.viewport = CD3DX12_VIEWPORT( m_hdr_backbuffer->Resource() );
		screen.scissor_rect = CD3DX12_RECT( 0, 0, screen.viewport.Width, screen.viewport.Height );
		m_framegraph.SetRes( screen );

		SSAOBuffer_Noisy ssao_texture;
		ssao_texture.res = m_ssao->Resource();
		ssao_texture.rtv = m_ssao->RTV()->HandleCPU();
		ssao_texture.srv = GetGPUHandle( m_ssao->SRV() );
		m_framegraph.SetRes( ssao_texture );

		SSAOTexture_Blurred ssao_blurred_texture;
		ssao_blurred_texture.res = m_ssao_blurred->Resource();
		ssao_blurred_texture.uav = GetGPUHandle( m_ssao_blurred->UAV() );
		ssao_blurred_texture.srv = GetGPUHandle( m_ssao_blurred->SRV() );
		m_framegraph.SetRes( ssao_blurred_texture );


		SSAOTexture_Transposed ssao_blurred_texture_horizontal;
		ssao_blurred_texture_horizontal.res = m_ssao_blurred_transposed->Resource();
		ssao_blurred_texture_horizontal.uav = GetGPUHandle( m_ssao_blurred_transposed->UAV() );
		ssao_blurred_texture_horizontal.srv = GetGPUHandle( m_ssao_blurred_transposed->SRV() );
		m_framegraph.SetRes( ssao_blurred_texture_horizontal );


		TonemapNodeSettings tm_settings;
		tm_settings.data.blend_luminance = false;
		tm_settings.data.lower_luminance_bound = m_tonemap_settings.min_luminance;
		tm_settings.data.upper_luminance_bound = m_tonemap_settings.max_luminance;
		m_framegraph.SetRes( tm_settings );

		::HBAOSettings hbao_settings;
		hbao_settings.data.render_target_size = DirectX::XMFLOAT2( screen.viewport.Width, screen.viewport.Height );
		hbao_settings.data.angle_bias = m_hbao_settings.angle_bias;
		hbao_settings.data.max_r = m_hbao_settings.max_r;
		hbao_settings.data.nsamples_per_direction = m_hbao_settings.nsamples_per_direction;
		m_framegraph.SetRes( hbao_settings );

		SDRBuffer backbuffer;
		{
			const auto& target = frame_ctx.render_target;
			backbuffer.resource = target.resource;
			backbuffer.rtv = target.rtv;
			backbuffer.viewport = target.viewport;
			backbuffer.scissor_rect = target.scissor_rect;
		}
		m_framegraph.SetRes( backbuffer );
	}

	m_framegraph.Run( *list_iface );

	list_iface->ResourceBarrier( 3, rtv_barriers );

	ThrowIfFailed( list_iface->Close() );

	graphics_cmd_lists.emplace_back( std::move( cmd_list ) );
}


DXGI_FORMAT SceneRenderer::GetTargetFormat( RenderMode mode ) const noexcept
{
	return m_back_buffer_format;
}


void SceneRenderer::SetTargetFormat( RenderMode mode, DXGI_FORMAT format )
{
	if ( mode != RenderMode::FullTonemapped )
		NOTIMPL;

	m_back_buffer_format = format;

	if ( m_back_buffer_format != DefaultBackbufferFormat )
		throw SnowEngineException( "unsupported rtv format" );

	m_framegraph.ConstructNode<ToneMapNode>( &m_framegraph, m_back_buffer_format, *m_device );
}


std::vector<RenderItem> SceneRenderer::CreateRenderitems( const Camera::Data& camera, const Scene& scene ) const
{
	if ( camera.type != Camera::Type::Perspective )
		NOTIMPL;

	DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH( camera.fov_y,
																camera.aspect_ratio,
																camera.near_plane,
																camera.far_plane );

	DirectX::BoundingFrustum main_bf( proj );

	DirectX::XMMATRIX view = DirectX::XMMatrixLookToLH( DirectX::XMLoadFloat3( &camera.pos ),
														DirectX::XMLoadFloat3( &camera.dir ),
														DirectX::XMLoadFloat3( &camera.up ) );
	DirectX::XMVECTOR det;
	main_bf.Transform( main_bf, DirectX::XMMatrixInverse( &det, view ) );

	std::vector<RenderItem> items;
	items.reserve( scene.StaticMeshInstanceSpan().size() );
	for ( const auto& mesh_instance : scene.StaticMeshInstanceSpan() )
	{
		if ( ! mesh_instance.IsEnabled() )
			continue;

		const StaticSubmesh& submesh = scene.AllStaticSubmeshes()[mesh_instance.Submesh()];
		const StaticMesh& geom = scene.AllStaticMeshes()[submesh.GetMesh()];
		if ( ! geom.IsLoaded() )
			continue;

		RenderItem item;
		item.ibv = geom.IndexBufferView();
		item.vbv = geom.VertexBufferView();

		const auto& submesh_draw_args = submesh.DrawArgs();

		item.index_count = submesh_draw_args.idx_cnt;
		item.index_offset = submesh_draw_args.start_index_loc;
		item.vertex_offset = submesh_draw_args.base_vertex_loc;

		const MaterialPBR& material = scene.AllMaterials()[mesh_instance.Material()];
		item.mat_cb = material.GPUConstantBuffer();
		item.mat_table = material.DescriptorTable();

		bool has_unloaded_texture = false;
		const auto& textures = material.Textures();
		for ( TextureID tex_id : { textures.base_color, textures.normal, textures.specular } )
		{
			if ( ! scene.AllTextures()[tex_id].IsLoaded() )
			{
				has_unloaded_texture = true;
				break;
			}
		}

		if ( has_unloaded_texture )
			continue;

		const ObjectTransform& tf = scene.AllTransforms()[mesh_instance.GetTransform()];
		item.tf_addr = tf.GPUView();

		DirectX::BoundingOrientedBox item_box;
		DirectX::BoundingOrientedBox::CreateFromBoundingBox( item_box, submesh.Box() );

		item_box.Transform( item_box, DirectX::XMLoadFloat4x4( &tf.Obj2World() ) );

		if ( item_box.Intersects( main_bf ) )
			items.push_back( item );

		return std::move( items );
	}
}

Skybox SceneRenderer::CreateSkybox( EnvMapID skybox_id, DescriptorTableID ibl_table, const Scene& scene ) const
{
	assert( scene.AllEnviromentMaps().has( skybox_id ) );

	const EnviromentMap& skybox = scene.AllEnviromentMaps()[skybox_id];

	Skybox framegraph_res;

	CubemapID cubemap_id = skybox.GetMap();

	const Cubemap* cubemap = scene.AllCubemaps().try_get( cubemap_id );
	if ( ! cubemap )
		throw SnowEngineException( "skybox does not have a cubemap attached" );

	if ( cubemap->IsLoaded() )
		framegraph_res.srv_skybox = skybox.GetSRV();
	else
		framegraph_res.srv_skybox.ptr = 0;

	{
		D3D12_GPU_DESCRIPTOR_HANDLE desc_table = m_descriptor_tables->GetTable( ibl_table )->gpu_handle;
		framegraph_res.srv_table = desc_table;
	}

	const ObjectTransform* tf = scene.AllTransforms().try_get( skybox.GetTransform() );
	if ( ! tf )
		throw SnowEngineException( "skybox does not have a transform attached" );

	framegraph_res.tf_cbv = tf->GPUView();

	framegraph_res.radiance_factor = skybox.GetRadianceFactor();

	return framegraph_res;
}
