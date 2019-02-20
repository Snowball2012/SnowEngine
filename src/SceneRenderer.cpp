// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "SceneRenderer.h"

#include "FramegraphResource.h"
#include "Framegraph.h"


void SceneRenderer::Draw( const SceneContext& ctx, RenderMode mode, const Target& target )
{
	assert( ctx.main_camera != CameraID::nullid );
	assert( ctx.scene != nullptr );

	if ( mode != RenderMode::FullTonemapped )
		NOTIMPL;

	m_framegraph.ClearResources();

	if ( m_framegraph.IsRebuildNeeded() )
		m_framegraph.Rebuild();

	Scene& scene = *ctx.scene;

	const Camera* main_camera = scene.AllCameras().try_get( ctx.main_camera );
	if ( ! main_camera )
		throw SnowEngineException( "no main camera" );

	std::vector<RenderItem> lighting_items = BuildRenderitems( main_camera->GetData(), scene );

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
	ThrowIfFailed( m_frame_allocators[m_frame_idx].Reset() );

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList. Reusing the command list reuses memory
	ThrowIfFailed( m_cmd_list->Reset( m_frame_allocators[m_frame_idx].GetAllocator(), nullptr ) );

	CD3DX12_RESOURCE_BARRIER rtv_barriers[9];
	rtv_barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition( m_fp_backbuffer->Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET );
	rtv_barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition( m_ambient_lighting->Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET );
	rtv_barriers[2] = CD3DX12_RESOURCE_BARRIER::Transition( m_normals->Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET );
	rtv_barriers[3] = CD3DX12_RESOURCE_BARRIER::Transition( m_ssao->Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET );
	rtv_barriers[4] = CD3DX12_RESOURCE_BARRIER::Transition( m_ssao_blurred->Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
	rtv_barriers[5] = CD3DX12_RESOURCE_BARRIER::Transition( m_ssao_blurred_transposed->Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
	
	{
		auto& sm_storage = m_framegraph.GetRes<ShadowMaps>();
		if ( ! sm_storage )
			throw SnowEngineException( "missing resource" );
		rtv_barriers[7] = CD3DX12_RESOURCE_BARRIER::Transition( sm_storage->res, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE );
		auto& pssm_storage = m_framegraph.GetRes<ShadowCascade>();
		if ( ! pssm_storage )
			throw SnowEngineException( "missing resource" );
		rtv_barriers[8] = CD3DX12_RESOURCE_BARRIER::Transition( pssm_storage->res, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE );
	}

	m_cmd_list->ResourceBarrier( 9, rtv_barriers );
	ID3D12DescriptorHeap* heaps[] = { m_descriptor_tables->CurrentGPUHeap().Get() };
	m_cmd_list->SetDescriptorHeaps( 1, heaps );

	{
		BindSkybox( main_camera->GetSkybox() );

		ForwardPassCB pass_cb{ m_forward_cb_provider.GetCBPointer() };
		m_framegraph.SetRes( pass_cb );

		HDRBuffer hdr_buffer;
		hdr_buffer.res = m_fp_backbuffer->Resource();
		hdr_buffer.rtv = m_fp_backbuffer->RTV()->HandleCPU();
		hdr_buffer.srv = GetGPUHandle( m_fp_backbuffer->SRV() );
		m_framegraph.SetRes( hdr_buffer );

		AmbientBuffer ambient_buffer;
		ambient_buffer.res = m_ambient_lighting->Resource();
		ambient_buffer.rtv = m_ambient_lighting->RTV()->HandleCPU();
		ambient_buffer.srv = GetGPUHandle( m_ambient_lighting->SRV() );
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
		screen.viewport = CD3DX12_VIEWPORT( m_fp_backbuffer->Resource() );
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
		backbuffer.resource = target.resource;
		backbuffer.rtv = target.rtv;
		backbuffer.viewport = target.viewport;
		backbuffer.scissor_rect = target.scissor_rect;
		m_framegraph.SetRes( backbuffer );
	}

	m_framegraph.Run( *m_cmd_list.Get() );

	m_cmd_list->ResourceBarrier( 3, rtv_barriers );

	ThrowIfFailed( m_cmd_list->Close() );

	ID3D12CommandList* cmd_lists[] = { m_cmd_list.Get() };
	m_graphics_queue->GetCmdQueue()->ExecuteCommandLists( _countof( cmd_lists ), cmd_lists );
}


std::vector<RenderItem> SceneRenderer::BuildRenderitems( const Camera::Data& camera, const Scene& scene )
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
