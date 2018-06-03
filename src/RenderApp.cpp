#include "stdafx.h"

#include "RenderApp.h"

#include "GeomGeneration.h"

#include "ForwardLightingPass.h"
#include "DepthOnlyPass.h"

#include <dxtk12/DDSTextureLoader.h>
#include <dxtk12/DirectXHelpers.h>

#include <d3d12sdklayers.h>

#include <future>

using namespace DirectX;

RenderApp::RenderApp( HINSTANCE hinstance, LPSTR cmd_line )
	: D3DApp( hinstance ), m_cmd_line( cmd_line )
{
	mMainWndCaption = L"Snow Engine";
	mClientWidth = 1920;
	mClientHeight = 1080;
}

RenderApp::~RenderApp() = default;

bool RenderApp::Initialize()
{
	if ( ! D3DApp::Initialize() )
		return false;

	if ( strlen( m_cmd_line ) != 0 )
		LoadModel( m_cmd_line );

	m_keyboard = std::make_unique<DirectX::Keyboard>();

	// create box, load to gpu through upload heap to default heap
	ThrowIfFailed( m_cmd_list->Reset( mDirectCmdListAlloc.Get(), nullptr ) );

	BuildDescriptorHeaps();
	LoadAndBuildTextures();
	BuildGeometry();
	BuildMaterials();
	BuildRenderItems();
	BuildFrameResources();
	BuildLights();
	BuildConstantBuffers();
	BuildPasses();

	ThrowIfFailed( m_cmd_list->Close() );
	ID3D12CommandList* cmd_lists[] = { m_cmd_list.Get() };
	m_cmd_queue->ExecuteCommandLists( _countof( cmd_lists ), cmd_lists );
	FlushCommandQueue();

	return true;
}

void RenderApp::OnResize()
{
	D3DApp::OnResize();

	// Need to recompute projection matrix
	XMMATRIX proj = XMMatrixPerspectiveFovLH( MathHelper::Pi / 4, AspectRatio(), 1.0f, 100000.0f );
	XMStoreFloat4x4( &m_scene.proj, proj );
}

void RenderApp::Update( const GameTimer& gt )
{
	UpdateAndWaitForFrameResource();

	UpdateShadowMapDescriptors();

	ReadKeyboardState( gt );

	// Update object constants if needed
	for ( auto& renderitem : m_scene.renderitems )
		UpdateRenderItem( renderitem, *m_cur_frame_resource->object_cb );

	// Update pass constants
	UpdatePassConstants( gt, *m_cur_frame_resource->pass_cb );
}

void RenderApp::ReadEventKeys()
{
	auto kb_state = m_keyboard->GetState();
	if ( kb_state.F3 )
		m_wireframe_mode = !m_wireframe_mode;
}

void RenderApp::ReadKeyboardState( const GameTimer& gt )
{
	auto kb_state = m_keyboard->GetState();

	constexpr float angle_per_second = XM_PIDIV2;

	float angle_step = angle_per_second * gt.DeltaTime();

	if ( kb_state.Left )
		m_sun_theta -= angle_step;
	if ( kb_state.Right )
		m_sun_theta += angle_step;
	if ( kb_state.Up )
		m_sun_phi += angle_step;
	if ( kb_state.Down )
		m_sun_phi -= angle_step;

	m_sun_phi = boost::algorithm::clamp( m_sun_phi, 0, DirectX::XM_PI );
	m_sun_theta = fmod( m_sun_theta, DirectX::XM_2PI );
}

void RenderApp::UpdatePassConstants( const GameTimer& gt, Utils::UploadBuffer<PassConstants>& pass_cb )
{
	const auto& cartesian = SphericalToCartesian( m_radius, m_phi, m_theta );

	XMVECTOR pos = XMVectorSet( cartesian.x, cartesian.y, cartesian.z, 1.0f );
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );

	XMMATRIX view = XMMatrixLookAtLH( pos, target, up );
	XMStoreFloat4x4( &m_scene.view, view );

	XMMATRIX proj = XMLoadFloat4x4( &m_scene.proj );
	const auto& vp = view * proj;

	PassConstants pc;
	XMStoreFloat4x4( &pc.ViewProj, XMMatrixTranspose( vp ) );
	pc.Proj = m_scene.proj;
	pc.View = m_scene.view;
	XMStoreFloat3( &pc.EyePosW, pos );

	UpdateLights( pc );

	pass_cb.CopyData( 0, pc );

	// shadow map pass constants
	{
		pc.ViewProj = m_scene.lights["sun"].data.shadow_map_matrix;
		pass_cb.CopyData( 1, pc );
	}

	// main pass
}

void RenderApp::UpdateLights( PassConstants& pc )
{
	// update sun dir
	auto& sun_light = m_scene.lights["sun"];
	{
		sun_light.data.dir = SphericalToCartesian( -1, m_sun_phi, m_sun_theta );

		const auto& cartesian = SphericalToCartesian( -100, m_sun_phi, m_sun_theta );

		XMVECTOR pos = XMVectorSet( cartesian.x, cartesian.y, cartesian.z, 1.0f );
		XMVECTOR target = XMVectorZero();
		XMVECTOR up = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );

		XMMATRIX view = XMMatrixLookAtLH( pos, target, up );
		XMMATRIX proj = XMMatrixOrthographicLH( 50, 50, 10.0f, 200 );
		const auto& viewproj = view * proj;
		XMStoreFloat4x4( &sun_light.data.shadow_map_matrix, XMMatrixTranspose( viewproj ) );
	}

	bc::static_vector<const LightConstants*, MAX_LIGHTS> parallel_lights, point_lights, spotlights;
	for ( const auto& light : m_scene.lights )
	{
		switch ( light.second.type )
		{
			case Light::Type::Parallel:
				parallel_lights.push_back( &light.second.data );
				break;
			case Light::Type::Point:
				point_lights.push_back( &light.second.data );
				break;
			case Light::Type::Spotlight:
				spotlights.push_back( &light.second.data );
				break;
			default:
				throw std::exception( "not implemented" );
		}
	}

	if ( parallel_lights.size() + point_lights.size() + spotlights.size() > MAX_LIGHTS )
		throw std::exception( "too many lights" );

	pc.n_parallel_lights = int( parallel_lights.size() );
	pc.n_point_lights = int( point_lights.size() );
	pc.n_spotlight_lights = int( spotlights.size() );

	size_t light_offset = 0;
	for ( const LightConstants* light : boost::range::join( parallel_lights,
															boost::range::join( point_lights,
																				spotlights ) ) )
	{
		pc.lights[light_offset++] = *light;
	}
}

void RenderApp::UpdateRenderItem( RenderItem& renderitem, Utils::UploadBuffer<ObjectConstants>& obj_cb )
{
	if ( renderitem.n_frames_dirty > 0 )
	{
		ObjectConstants obj_constants;
		XMStoreFloat4x4( &obj_constants.model, XMMatrixTranspose( XMLoadFloat4x4( &renderitem.world_mat ) ) );
		XMStoreFloat4x4( &obj_constants.model_inv_transpose, XMMatrixTranspose( InverseTranspose( XMLoadFloat4x4( &renderitem.world_mat ) ) ) );
		obj_cb.CopyData( renderitem.cb_idx, obj_constants );
		renderitem.n_frames_dirty--;
	}
}

void RenderApp::UpdateDynamicGeometry( Utils::UploadBuffer<Vertex>& dynamic_vb )
{
	NOTIMPL;
}

void RenderApp::UpdateAndWaitForFrameResource()
{
	m_cur_fr_idx = ( m_cur_fr_idx + 1 ) % num_frame_resources;
	m_cur_frame_resource = m_frame_resources.data() + m_cur_fr_idx;

	// wait for gpu to complete (i - 2)th frame;
	if ( m_cur_frame_resource->fence != 0 && mFence->GetCompletedValue() < m_cur_frame_resource->fence )
	{
		HANDLE event_handle = CreateEventEx( nullptr, nullptr, false, EVENT_ALL_ACCESS );
		ThrowIfFailed( mFence->SetEventOnCompletion( m_cur_frame_resource->fence, event_handle ) );
		WaitForSingleObject( event_handle, INFINITE );
		CloseHandle( event_handle );
	}
}


void RenderApp::Draw( const GameTimer& gt )
{
	// Reuse memory associated with command recording
	// We can only reset when the associated command lists have finished execution on GPU
	m_cur_frame_resource->fence = ++m_current_fence;
	m_cmd_queue->Signal( mFence.Get(), m_current_fence );

	for ( auto& allocator : m_cur_frame_resource->cmd_list_allocs )
		ThrowIfFailed( allocator->Reset() );

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList. Reusing the command list reuses memory
	ThrowIfFailed( m_sm_cmd_lst->Reset( m_cur_frame_resource->cmd_list_allocs[1].Get(), m_do_pso.Get() ) );
	ThrowIfFailed( m_cmd_list->Reset( m_cur_frame_resource->cmd_list_allocs[0].Get(), m_forward_pso_main.Get() ) );

	auto forward_cmd_list_filled = std::async( [&]()
	{
		m_cmd_list->ResourceBarrier( 1, &CD3DX12_RESOURCE_BARRIER::Transition( CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET ) );
		
		ID3D12DescriptorHeap* heaps[] = { m_srv_heap->GetInterface() };
		m_cmd_list->SetDescriptorHeaps( 1, heaps );

		// clear the back buffer and depth
		const float bgr_color[4] = { 0.8f, 0.83f, 0.9f };
		m_cmd_list->ClearRenderTargetView( CurrentBackBufferView(), bgr_color, 0, nullptr );
		m_cmd_list->ClearDepthStencilView( DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr );
		
		ForwardLightingPass::Context forward_ctx
		{
			&m_scene,
			CurrentBackBufferView(),
			DepthStencilView(),
			m_scene.lights["sun"].shadow_map->srv->HandleGPU(),
			m_cur_frame_resource->pass_cb->Resource(),
			0,
			m_cur_frame_resource->object_cb->Resource()
		};
		m_cmd_list->RSSetViewports( 1, &mScreenViewport );
		m_cmd_list->RSSetScissorRects( 1, &mScissorRect );


		m_forward_pass->Draw( forward_ctx, m_wireframe_mode, *m_cmd_list.Get() );

		// swap transition
		m_cmd_list->ResourceBarrier( 1, &CD3DX12_RESOURCE_BARRIER::Transition( CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT ) );

		// done
		ThrowIfFailed( m_cmd_list->Close() );		
	} );

	{
		// shadow map viewport and scissor rect
		D3D12_VIEWPORT sm_viewport;
		{
			sm_viewport.TopLeftX = 0;
			sm_viewport.TopLeftY = 0;
			sm_viewport.Height = 4096;
			sm_viewport.Width = 4096;
			sm_viewport.MinDepth = 0;
			sm_viewport.MaxDepth = 1;
		}
		D3D12_RECT sm_scissor;
		{
			sm_scissor.bottom = 4096;
			sm_scissor.left = 0;
			sm_scissor.right = 4096;
			sm_scissor.top = 0;
		}

		m_sm_cmd_lst->RSSetViewports( 1, &sm_viewport );
		m_sm_cmd_lst->RSSetScissorRects( 1, &sm_scissor );

		m_sm_cmd_lst->ResourceBarrier( 1, &CD3DX12_RESOURCE_BARRIER::Transition( m_scene.lights["sun"].shadow_map->texture_gpu.Get(),
																				 D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
																				 D3D12_RESOURCE_STATE_DEPTH_WRITE ) );

		m_sm_cmd_lst->ClearDepthStencilView( m_scene.lights["sun"].shadow_map->dsv->HandleCPU(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr );

		// passes
		DepthOnlyPass::Context shadow_map_gen_ctx
		{
			&m_scene,
			m_scene.lights["sun"].shadow_map->dsv->HandleCPU(),
			m_cur_frame_resource->pass_cb->Resource(),
			1,
			m_cur_frame_resource->object_cb->Resource()
		};


		m_depth_pass->Draw( shadow_map_gen_ctx, *m_sm_cmd_lst.Get() );

		m_sm_cmd_lst->ResourceBarrier( 1, &CD3DX12_RESOURCE_BARRIER::Transition( m_scene.lights["sun"].shadow_map->texture_gpu.Get(),
																				 D3D12_RESOURCE_STATE_DEPTH_WRITE,
																				 D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE ) );

		ThrowIfFailed( m_sm_cmd_lst->Close() );
	}

	// get forward cmd list
	forward_cmd_list_filled.wait();	

	ID3D12CommandList* cmd_lists[] = { m_sm_cmd_lst.Get(), m_cmd_list.Get() };
	m_cmd_queue->ExecuteCommandLists( _countof( cmd_lists ), cmd_lists );

	// swap buffers
	ThrowIfFailed( mSwapChain->Present( 0, 0 ) );
	mCurrBackBuffer = ( mCurrBackBuffer + 1 ) % SwapChainBufferCount;

	FlushCommandQueue();
}

void RenderApp::UpdateShadowMapDescriptors()
{
	for ( auto& shadow_map : m_cur_frame_resource->shadow_maps )
		m_scene.lights[shadow_map.first].shadow_map = &shadow_map.second;
}

void RenderApp::OnMouseDown( WPARAM btn_state, int x, int y )
{
	m_last_mouse_pos.x = x;
	m_last_mouse_pos.y = y;

	SetCapture( mhMainWnd );
}

void RenderApp::OnMouseUp( WPARAM btn_state, int x, int y )
{
	ReleaseCapture();
}

void RenderApp::OnMouseMove( WPARAM btnState, int x, int y )
{
	if ( ( btnState & MK_LBUTTON ) != 0 )
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians( 0.25f*static_cast<float>( x - m_last_mouse_pos.x ) );
		float dy = XMConvertToRadians( 0.25f*static_cast<float>( y - m_last_mouse_pos.y ) );

		// Update angles based on input to orbit camera around box.
		m_theta -= dx;
		m_phi -= dy;

		// Restrict the angle phi.
		m_phi = MathHelper::Clamp( m_phi, 0.1f, MathHelper::Pi - 0.1f );
	}
	else if ( ( btnState & MK_RBUTTON ) != 0 )
	{
		// Make each pixel correspond to 0.005 unit in the scene.
		float dx = 0.005f*static_cast<float>( x - m_last_mouse_pos.x );
		float dy = 0.005f*static_cast<float>( y - m_last_mouse_pos.y );

		// Update the camera radius based on input.
		m_radius += ( dx - dy ) * m_radius;

		// Restrict the radius.
		m_radius = MathHelper::Clamp( m_radius, 3.0f, 100000.0f );
	}

	m_last_mouse_pos.x = x;
	m_last_mouse_pos.y = y;
}

void RenderApp::BuildPasses()
{
	ForwardLightingPass::BuildData( BuildStaticSamplers(), mBackBufferFormat, mDepthStencilFormat, *m_d3d_device.Get(),
									m_forward_pso_main, m_forward_pso_wireframe, m_forward_root_signature );
	m_forward_pass = std::make_unique<ForwardLightingPass>( m_forward_pso_main.Get(), m_forward_pso_wireframe.Get(), m_forward_root_signature.Get() );


	DepthOnlyPass::BuildData( mDepthStencilFormat, 5000.0f, true, *m_d3d_device.Get(),
							  m_do_pso, m_do_root_signature );
	m_depth_pass = std::make_unique<DepthOnlyPass>( m_do_pso.Get(), m_do_root_signature.Get() );

	ThrowIfFailed( m_d3d_device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_frame_resources[0].cmd_list_allocs[1].Get(), // Associated command allocator
		nullptr, // Initial PipelineStateObject
		IID_PPV_ARGS( m_sm_cmd_lst.GetAddressOf() ) ) );
	m_sm_cmd_lst->Close();
}

void RenderApp::BuildDescriptorHeaps()
{
	// srv heap
	{
		D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc{};
		srv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		srv_heap_desc.NumDescriptors = 3 + UINT( m_imported_scene.textures.size() ) + 1 * num_frame_resources;

		ComPtr<ID3D12DescriptorHeap> srv_heap;
		ThrowIfFailed( m_d3d_device->CreateDescriptorHeap( &srv_heap_desc, IID_PPV_ARGS( &srv_heap ) ) );
		m_srv_heap = std::make_unique<DescriptorHeap>( std::move( srv_heap ), mCbvSrvUavDescriptorSize );
	}
	// secondary dsv heap
	{
		D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc{};
		dsv_heap_desc.NumDescriptors = 1 * num_frame_resources; // shadow maps
		dsv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

		ComPtr<ID3D12DescriptorHeap> dsv_heap;
		ThrowIfFailed( m_d3d_device->CreateDescriptorHeap( &dsv_heap_desc, IID_PPV_ARGS( &dsv_heap ) ) );
		m_dsv_heap = std::make_unique<DescriptorHeap>( std::move( dsv_heap ), mDsvDescriptorSize );
	}
}

void RenderApp::BuildConstantBuffers()
{
	for ( int i = 0; i < num_frame_resources; ++i )
	{
		// object cb
		m_frame_resources[i].object_cb = std::make_unique<Utils::UploadBuffer<ObjectConstants>>( m_d3d_device.Get(), UINT( m_scene.renderitems.size() ), true );
		// pass cb
		m_frame_resources[i].pass_cb = std::make_unique<Utils::UploadBuffer<PassConstants>>( m_d3d_device.Get(), num_passes, true );
	}
}

void RenderApp::BuildGeometry()
{
	// static
	{
		auto& submeshes = LoadStaticGeometry<DXGI_FORMAT_R32_UINT>( "main", m_imported_scene.vertices, m_imported_scene.indices, m_cmd_list.Get() );

		for ( const auto& cpu_submesh : m_imported_scene.submeshes )
		{
			SubmeshGeometry submesh;
			submesh.IndexCount = UINT( cpu_submesh.nindices );
			submesh.StartIndexLocation = UINT( cpu_submesh.index_offset );
			submesh.BaseVertexLocation = 0;
			submeshes[cpu_submesh.name] = submesh;
		}
	}
}

void RenderApp::LoadAndBuildTextures()
{
	LoadStaticDDSTexture( L"resources/textures/WoodCrate01.dds", "placeholder_albedo" );
	LoadStaticDDSTexture( L"resources/textures/default_deriv_normal.dds", "placeholder_normal" );
	LoadStaticDDSTexture( L"resources/textures/default_spec.dds", "placeholder_specular" );
	for ( size_t i = 0; i < m_imported_scene.textures.size(); ++i )
		LoadStaticDDSTexture( std::wstring( m_imported_scene.textures[i].begin(), m_imported_scene.textures[i].end() ).c_str(), m_imported_scene.textures[i] );
}

void RenderApp::LoadStaticDDSTexture( const wchar_t* filename, const std::string& name )
{
	auto& texture = m_scene.textures.emplace( name, StaticTexture() ).first->second;

	std::unique_ptr<uint8_t[]> dds_data;
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	ThrowIfFailed( DirectX::LoadDDSTextureFromFile( m_d3d_device.Get(), filename, texture.texture_gpu.GetAddressOf(), dds_data, subresources ) );

	size_t total_size = GetRequiredIntermediateSize( texture.texture_gpu.Get(), 0, UINT( subresources.size() ) );

	// create upload buffer
	ThrowIfFailed( m_d3d_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_UPLOAD ),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer( total_size ),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS( texture.texture_uploader.GetAddressOf() ) ) ); // TODO: not always committed?

	// base resource is now in COPY_DEST STATE, no barrier required 
	UpdateSubresources( m_cmd_list.Get(), texture.texture_gpu.Get(), texture.texture_uploader.Get(), 0, 0, UINT( subresources.size() ), subresources.data() );
	m_cmd_list->ResourceBarrier( 1, &CD3DX12_RESOURCE_BARRIER::Transition( texture.texture_gpu.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ ) );

	texture.srv = std::make_unique<Descriptor>( m_srv_heap->AllocateDescriptor() );
	CreateShaderResourceView( m_d3d_device.Get(), texture.texture_gpu.Get(), texture.srv->HandleCPU() );
}

void RenderApp::BuildMaterials()
{
	{
		auto& placeholder_material = m_scene.materials.emplace( "placeholder", StaticMaterial() ).first->second;
		auto& mat_data = placeholder_material.mat_constants;
		mat_data.mat_transform = MathHelper::Identity4x4();
		mat_data.diffuse_fresnel = XMFLOAT3( 0.03f, 0.03f, 0.03f );
		placeholder_material.base_color_desc = m_scene.textures["placeholder_albedo"].srv->HandleGPU();
		placeholder_material.normal_map_desc = m_scene.textures["placeholder_normal"].srv->HandleGPU();
		placeholder_material.specular_desc = m_scene.textures["placeholder_specular"].srv->HandleGPU();

		placeholder_material.LoadToGPU( *m_d3d_device.Get(), *m_cmd_list.Get() );
	}

	for ( int i = 0; i < m_imported_scene.materials.size(); ++i )
	{
		auto& material = m_scene.materials.emplace( m_imported_scene.materials[i].first, StaticMaterial() ).first->second;
		auto& mat_data = material.mat_constants;
		mat_data.mat_transform = MathHelper::Identity4x4();
		mat_data.diffuse_fresnel = XMFLOAT3( 0.03f, 0.03f, 0.03f );
		material.base_color_desc = m_scene.textures[m_imported_scene.textures[m_imported_scene.materials[i].second.base_color_tex_idx]].srv->HandleGPU();
		material.normal_map_desc = m_scene.textures[m_imported_scene.textures[m_imported_scene.materials[i].second.normal_tex_idx]].srv->HandleGPU();

		int spec_map_idx = m_imported_scene.materials[i].second.specular_tex_idx;
		if ( spec_map_idx < 0 )
			material.specular_desc = m_scene.textures["placeholder_specular"].srv->HandleGPU();
		else
			material.specular_desc = m_scene.textures[m_imported_scene.textures[spec_map_idx]].srv->HandleGPU();

		material.LoadToGPU( *m_d3d_device.Get(), *m_cmd_list.Get() );
	}
}

void RenderApp::BuildLights()
{
	auto& parallel_light = m_scene.lights.emplace( "sun", Light{ Light::Type::Parallel } ).first->second;
	LightConstants& data = parallel_light.data;
	data.strength = XMFLOAT3( 5.f, 5.f, 5.f );
	data.dir = XMFLOAT3( 0.172f, -0.818f, -0.549f );
	XMFloat3Normalize( data.dir );
	parallel_light.shadow_map = nullptr;
	parallel_light.is_dynamic = true;

	for ( auto& frame_res : m_frame_resources )
	{
		auto& shadow_map = frame_res.shadow_maps["sun"];
		CreateTexture( shadow_map );
		D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc;
		{
			dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsv_desc.Format = mDepthStencilFormat;
			dsv_desc.Texture2D.MipSlice = 0;
			dsv_desc.Flags = D3D12_DSV_FLAG_NONE;
		}
		shadow_map.dsv = std::make_unique<Descriptor>( std::move( m_dsv_heap->AllocateDescriptor() ) );
		m_d3d_device->CreateDepthStencilView( shadow_map.texture_gpu.Get(), &dsv_desc, shadow_map.dsv->HandleCPU() );
	}
}

namespace
{
	void initFromSubmesh( const SubmeshGeometry& submesh, RenderItem& renderitem )
	{
		renderitem.index_count = submesh.IndexCount;
		renderitem.index_offset = submesh.StartIndexLocation;
		renderitem.vertex_offset = submesh.BaseVertexLocation;
	};

	void initFromSubmesh( const ImportedScene::Submesh& submesh, RenderItem& renderitem )
	{
		renderitem.index_count = uint32_t( submesh.nindices );
		renderitem.index_offset = uint32_t( submesh.index_offset );
		renderitem.vertex_offset = 0;
		renderitem.world_mat = submesh.transform;
	};
}

void RenderApp::BuildRenderItems()
{
	auto init_from_submesh = []( const auto& submesh, RenderItem& renderitem )
	{
		renderitem.n_frames_dirty = num_frame_resources;
		initFromSubmesh( submesh, renderitem );
	};

	m_scene.renderitems.reserve( m_imported_scene.submeshes.size() );

	size_t idx = 0;
	for ( const auto& submesh : m_imported_scene.submeshes )
	{
		RenderItem item;
		item.cb_idx = int( idx++ );
		item.geom = &m_scene.static_geometry["main"];
		const int material_idx = submesh.material_idx;
		std::string material_name;
		if ( material_idx < 0 )
			material_name = "placeholder";
		else
			material_name = m_imported_scene.materials[material_idx].first;

		item.material = &m_scene.materials[material_name];
		init_from_submesh( submesh, item );
		m_scene.renderitems.push_back( item );
	}
}

template<DXGI_FORMAT index_format, class VCRange, class ICRange>
std::unordered_map<std::string, SubmeshGeometry>& RenderApp::LoadStaticGeometry( std::string name, const VCRange& vertices, const ICRange& indices, ID3D12GraphicsCommandList* cmd_list )
{
	_ASSERTE( cmd_list );

	using vertex_type = decltype( *vertices.data() );
	using index_type = decltype( *indices.data() );
	static_assert( ( index_format == DXGI_FORMAT_R16_UINT && sizeof( index_type ) == 2 )
				   || ( index_format == DXGI_FORMAT_R32_UINT && sizeof( index_type ) == 4 ), "Wrong index type in LoadGeometry" ); // todo: enum -> sizeof map check

	const UINT vb_byte_size = UINT( vertices.size() ) * sizeof( vertex_type );
	const UINT ib_byte_size = UINT( indices.size() ) * sizeof( index_type );

	auto& new_geom = m_scene.static_geometry.emplace( name, MeshGeometry() ).first->second;
	new_geom.Name = std::move( name );

	ThrowIfFailed( D3DCreateBlob( vb_byte_size, &new_geom.VertexBufferCPU ) );
	CopyMemory( new_geom.VertexBufferCPU->GetBufferPointer(), vertices.data(), vb_byte_size );

	ThrowIfFailed( D3DCreateBlob( ib_byte_size, &new_geom.IndexBufferCPU ) );
	CopyMemory( new_geom.IndexBufferCPU->GetBufferPointer(), indices.data(), ib_byte_size );

	new_geom.VertexBufferGPU = Utils::CreateDefaultBuffer( m_d3d_device.Get(), cmd_list, vertices.data(), vb_byte_size, new_geom.VertexBufferUploader );
	new_geom.IndexBufferGPU = Utils::CreateDefaultBuffer( m_d3d_device.Get(), cmd_list, indices.data(), ib_byte_size, new_geom.IndexBufferUploader );

	new_geom.VertexByteStride = sizeof( vertex_type );
	new_geom.VertexBufferByteSize = vb_byte_size;
	new_geom.IndexFormat = index_format;
	new_geom.IndexBufferByteSize = ib_byte_size;

	return new_geom.DrawArgs;
}

template<DXGI_FORMAT index_format, class ICRange>
void RenderApp::LoadDynamicGeometryIndices( const ICRange& indices, ID3D12GraphicsCommandList* cmd_list )
{
	using index_type = decltype( *indices.data() );
	static_assert( index_format == DXGI_FORMAT_R16_UINT && sizeof( index_type ) == 2, "Wrong index type in LoadGeometry" ); // todo: enum -> sizeof map check

	const UINT ib_byte_size = UINT( indices.size() ) * sizeof( index_type );

	auto& new_geom = m_dynamic_geometry;

	ThrowIfFailed( D3DCreateBlob( ib_byte_size, &new_geom.IndexBufferCPU ) );
	CopyMemory( new_geom.IndexBufferCPU->GetBufferPointer(), indices.data(), ib_byte_size );

	new_geom.IndexBufferGPU = Utils::CreateDefaultBuffer( m_d3d_device.Get(), cmd_list, indices.data(), ib_byte_size, new_geom.IndexBufferUploader );

	new_geom.IndexFormat = index_format;
	new_geom.IndexBufferByteSize = ib_byte_size;
}

void RenderApp::BuildFrameResources()
{
	for ( int i = 0; i < num_frame_resources; ++i )
		m_frame_resources.emplace_back( m_d3d_device.Get(), 2, 1, UINT( m_scene.renderitems.size() ), 0 );
}

void RenderApp::CreateTexture( Texture& texture )
{
	// temp. only shadow maps for now
	constexpr UINT width = 4096;
	constexpr UINT height = 4096;

	CD3DX12_RESOURCE_DESC tex_desc = CD3DX12_RESOURCE_DESC::Tex2D( DXGI_FORMAT_R24G8_TYPELESS, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL );
	D3D12_CLEAR_VALUE opt_clear;
	opt_clear.Format = mDepthStencilFormat;
	opt_clear.DepthStencil.Depth = 1.0f;
	opt_clear.DepthStencil.Stencil = 0;

	ThrowIfFailed( m_d3d_device->CreateCommittedResource( &CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT ), D3D12_HEAP_FLAG_NONE,
														&tex_desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
														&opt_clear, IID_PPV_ARGS( &texture.texture_gpu ) ) );

	texture.srv = std::make_unique<Descriptor>( std::move( m_srv_heap->AllocateDescriptor() ) );

	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
	{
		srv_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = 1;
	}
	m_d3d_device->CreateShaderResourceView( texture.texture_gpu.Get(), &srv_desc, texture.srv->HandleCPU() );
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> RenderApp::BuildStaticSamplers() const
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP ); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP ); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP ); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP ); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8 );                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8 );                                // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC shadow(
		6,
		D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		0.0f,
		16,
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE
		);
	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp, shadow };
}

LRESULT RenderApp::MsgProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	switch ( msg )
	{
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYUP:
			Keyboard::ProcessMessage( msg, wParam, lParam );
			ReadEventKeys();
			break;
	}

	return D3DApp::MsgProc( hwnd, msg, wParam, lParam );
}

void RenderApp::LoadModel( const std::string& filename )
{
	LoadFbxFromFile( filename, m_imported_scene );
}
