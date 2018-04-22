#include "stdafx.h"

#include "RenderApp.h"

#include "GeomGeneration.h"

#include <dxtk12/DDSTextureLoader.h>

using namespace DirectX;

RenderApp::RenderApp( HINSTANCE hinstance, LPSTR cmd_line )
	: D3DApp( hinstance ), m_cmd_line( cmd_line )
{
	mMainWndCaption = L"Snow Engine";
}

bool RenderApp::Initialize()
{
	if ( ! D3DApp::Initialize() )
		return false;

	m_keyboard = std::make_unique<DirectX::Keyboard>();

	// create box, load to gpu through upload heap to default heap
	ThrowIfFailed( mCommandList->Reset( mDirectCmdListAlloc.Get(), nullptr ) );

	BuildGeometry();
	LoadAndBuildTextures();
	BuildMaterials();
	BuildLights();
	BuildRenderItems();
	BuildFrameResources();	
	BuildDescriptorHeaps();
	BuildConstantBuffers();
	BuildRootSignature();
	BuildShadersAndInputLayout();	
	BuildPSO();

	ThrowIfFailed( mCommandList->Close() );
	ID3D12CommandList* cmd_lists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists( _countof( cmd_lists ), cmd_lists );

	FlushCommandQueue();

	return true;
}

void RenderApp::OnResize()
{
	D3DApp::OnResize();

	// Need to recompute projection matrix
	XMMATRIX proj = XMMatrixPerspectiveFovLH( MathHelper::Pi / 4, AspectRatio(), 1.0f, 1000.0f );
	XMStoreFloat4x4( &m_proj, proj );
}

void RenderApp::Update( const GameTimer& gt )
{
	UpdateAndWaitForFrameResource();

	ReadKeyboardState( gt );

	UpdateWaves( gt );
	UpdateDynamicGeometry( *m_cur_frame_resource->dynamic_geom_vb );

	// Update object constants if needed
	for ( auto& renderitem : m_renderitems )
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
	XMStoreFloat4x4( &m_view, view );

	XMMATRIX proj = XMLoadFloat4x4( &m_proj );
	const auto& vp = view * proj;

	PassConstants pc;
	XMStoreFloat4x4( &pc.ViewProj, XMMatrixTranspose( vp ) );
	pc.Proj = m_proj;
	pc.View = m_view;
	XMStoreFloat3( &pc.EyePosW, pos );

	UpdateLights( pc );

	pass_cb.CopyData( 0, pc );
}

void RenderApp::UpdateLights( PassConstants& pc )
{
	// update sun dir
	auto& sun_light = m_lights["sun"];
	sun_light.data.dir = SphericalToCartesian( -1, m_sun_phi, m_sun_theta );

	bc::static_vector<const LightConstants*, MAX_LIGHTS> parallel_lights, point_lights, spotlights;
	for ( const auto& light : m_lights )
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
		obj_cb.CopyData( renderitem.cb_idx, obj_constants );
		renderitem.n_frames_dirty--;
	}
}

void RenderApp::UpdateDynamicGeometry( Utils::UploadBuffer<Vertex>& dynamic_vb )
{
	for ( size_t i = 0; i < m_waves_cpu_vertices.size(); ++i )
		dynamic_vb.CopyData( int( i ), m_waves_cpu_vertices[i] );

	m_dynamic_geometry.VertexBufferGPU = dynamic_vb.Resource();
}

void RenderApp::UpdateWaves( const GameTimer& gt )
{
	const float t = gt.TotalTime();

	for ( auto& vertex : m_waves_cpu_vertices )
		vertex.pos = XMFLOAT3( vertex.pos.x, 1.f*( sinf( 4.f * vertex.pos.x + 3.f*t ) + cosf( vertex.pos.y + 1.f*t ) + 0.3f*cosf( 4.f * vertex.pos.z + 2.f*t ) ), vertex.pos.z );

	GeomGeneration::CalcAverageNormals( m_waves_cpu_indices,
										m_waves_cpu_vertices | boost::adaptors::transformed( []( Vertex& vertex )->const XMFLOAT3&{ return vertex.pos; } ),
										[this]( size_t idx )->XMFLOAT3& { return m_waves_cpu_vertices[idx].normal; } );
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
	m_cur_frame_resource->fence = ++mCurrentFence;
	mCommandQueue->Signal( mFence.Get(), mCurrentFence );

	ThrowIfFailed( m_cur_frame_resource->cmd_list_alloc->Reset() );

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList. Reusing the command list reuses memory
	ThrowIfFailed( mCommandList->Reset( m_cur_frame_resource->cmd_list_alloc.Get(), ( m_wireframe_mode ? m_pso_wireframe : m_pso_main ).Get() ) );

	// state transition( for back buffer )
	mCommandList->ResourceBarrier( 1, &CD3DX12_RESOURCE_BARRIER::Transition( CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET ) );

	// viewport and scissor rect
	mCommandList->RSSetViewports( 1, &mScreenViewport );
	mCommandList->RSSetScissorRects( 1, &mScissorRect );

	// clear the back buffer and depth
	const float bgr_color[4] = { 0.8f, 0.83f, 0.9f };
	mCommandList->ClearRenderTargetView( CurrentBackBufferView(), bgr_color, 0, nullptr );
	mCommandList->ClearDepthStencilView( DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr );

	// passes
	Draw_MainPass( mCommandList.Get() );

	// swap transition
	mCommandList->ResourceBarrier( 1, &CD3DX12_RESOURCE_BARRIER::Transition( CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT ) );

	// done
	ThrowIfFailed( mCommandList->Close() );

	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists( _countof( cmdLists ), cmdLists );

	// swap buffers
	ThrowIfFailed( mSwapChain->Present( 0, 0 ) );
	mCurrBackBuffer = ( mCurrBackBuffer + 1 ) % SwapChainBufferCount;

	FlushCommandQueue();
}

void RenderApp::Draw_MainPass( ID3D12GraphicsCommandList* cmd_list )
{
	// set render target
	cmd_list->OMSetRenderTargets( 1, &CurrentBackBufferView(), true, &DepthStencilView() );

	cmd_list->SetGraphicsRootSignature( m_root_signature.Get() );
	cmd_list->SetGraphicsRootConstantBufferView( 2, m_cur_frame_resource->pass_cb->Resource()->GetGPUVirtualAddress() );

	const auto obj_cb_adress = m_cur_frame_resource->object_cb->Resource()->GetGPUVirtualAddress();
	const auto obj_cb_size = d3dUtil::CalcConstantBufferByteSize( sizeof( ObjectConstants ) );
	for ( const auto& render_item : m_renderitems )
	{
		cmd_list->SetGraphicsRootConstantBufferView( 0, obj_cb_adress + render_item.cb_idx * obj_cb_size );
		cmd_list->SetGraphicsRootConstantBufferView( 1, render_item.material->cb_gpu->GetGPUVirtualAddress() );
		cmd_list->IASetVertexBuffers( 0, 1, &render_item.geom->VertexBufferView() );
		cmd_list->IASetIndexBuffer( &render_item.geom->IndexBufferView() );
		cmd_list->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
		cmd_list->DrawIndexedInstanced( render_item.index_count, 1, render_item.index_offset, render_item.vertex_offset, 0 );
	}
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
		m_radius += (dx - dy) * m_radius;

		// Restrict the radius.
		m_radius = MathHelper::Clamp( m_radius, 3.0f, 1000.0f );
	}

	m_last_mouse_pos.x = x;
	m_last_mouse_pos.y = y;
}

void RenderApp::BuildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc;
	srv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srv_heap_desc.NumDescriptors = 1;
	srv_heap_desc.NodeMask = 0;
	ThrowIfFailed( md3dDevice->CreateDescriptorHeap( &srv_heap_desc, IID_PPV_ARGS( &m_srv_heap ) ) );
}

void RenderApp::BuildConstantBuffers()
{
	for ( int i = 0; i < num_frame_resources; ++i )
	{
		// object cb
		m_frame_resources[i].object_cb = std::make_unique<Utils::UploadBuffer<ObjectConstants>>( md3dDevice.Get(), UINT( m_renderitems.size() ), true );
		// pass cb
		m_frame_resources[i].pass_cb = std::make_unique<Utils::UploadBuffer<PassConstants>>( md3dDevice.Get(), num_passes, true );
	}
}

void RenderApp::BuildRootSignature()
{
	constexpr int nparams = 3;
	CD3DX12_ROOT_PARAMETER slot_root_parameter[nparams];

	for ( int i = 0; i < nparams; ++i )
		slot_root_parameter[i].InitAsConstantBufferView( i );

	CD3DX12_ROOT_SIGNATURE_DESC root_sig_desc( nparams, slot_root_parameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT );

	ComPtr<ID3DBlob> serialized_root_sig = nullptr;
	ComPtr<ID3DBlob> error_blob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature( &root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1, serialized_root_sig.GetAddressOf(), error_blob.GetAddressOf() );

	if ( error_blob )
	{
		OutputDebugStringA( (char*)error_blob->GetBufferPointer() );
	}
	ThrowIfFailed( hr );

	ThrowIfFailed( md3dDevice->CreateRootSignature(
		0,
		serialized_root_sig->GetBufferPointer(),
		serialized_root_sig->GetBufferSize(),
		IID_PPV_ARGS( &m_root_signature ) ) );
}

void RenderApp::BuildShadersAndInputLayout()
{
	m_vs_bytecode = Utils::LoadBinary( L"shaders/vs.cso" );
	m_ps_bytecode = Utils::LoadBinary( L"shaders/ps.cso" );

	m_input_layout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

namespace
{
	float hillsHeight( float x, float z )
	{
		return 0.3f*( z*sinf( 0.1f*x ) + x*cosf( 0.1f*z ) );
	}
}

void RenderApp::BuildGeometry()
{
	constexpr size_t grid_nx = 50;
	constexpr size_t grid_ny = 50;

	// static
	{
		auto grid = GeomGeneration::MakeArrayGrid<grid_nx, grid_ny>( 160, 160 );

		for ( auto& vertex : grid.first )
		{
			vertex.pos.y = hillsHeight( vertex.pos.x, vertex.pos.z );
		}

		GeomGeneration::CalcAverageNormals( grid.second,
											grid.first | boost::adaptors::transformed( []( Vertex& vertex )->const XMFLOAT3&{ return vertex.pos; } ),
											[&grid]( size_t idx )->XMFLOAT3&{ return grid.first[idx].normal; } );

		auto& submeshes = LoadStaticGeometry<DXGI_FORMAT_R16_UINT>( "land_grid", grid.first, grid.second, mCommandList.Get() );

		SubmeshGeometry submesh;
		submesh.IndexCount = UINT( grid.second.size() );
		submesh.StartIndexLocation = 0;
		submesh.BaseVertexLocation = 0;

		submeshes["grid"] = submesh;
	}

	// dynamic
	{
		constexpr size_t waves_indices_count = GeomGeneration::GetGridNIndices( grid_nx, grid_ny );
		m_waves_cpu_indices.reserve( waves_indices_count );

		const size_t vertex_cnt = grid_nx * grid_ny;
		m_waves_cpu_vertices.reserve( vertex_cnt );

		GeomGeneration::MakeGrid( grid_nx, grid_ny, 160, 160
			, [&]( float x, float y )
			{
			m_waves_cpu_vertices.emplace_back( Vertex{ DirectX::XMFLOAT3( x, 0, y ) } );
			}
			, [&]( size_t idx )
			{
				m_waves_cpu_indices.emplace_back( uint16_t( idx ) );
			} );

		LoadDynamicGeometryIndices<DXGI_FORMAT_R16_UINT>( m_waves_cpu_indices, mCommandList.Get() );

		m_dynamic_geometry.Name = "main";
		m_dynamic_geometry.VertexByteStride = sizeof( Vertex );
		m_dynamic_geometry.VertexBufferByteSize = UINT( m_dynamic_geometry.VertexByteStride * m_waves_cpu_vertices.size() );

		SubmeshGeometry submesh;
		submesh.IndexCount = UINT( waves_indices_count );
		submesh.StartIndexLocation = 0;
		submesh.BaseVertexLocation = 0;

		m_dynamic_geometry.DrawArgs["waves"] = submesh;
	}
}

void RenderApp::LoadAndBuildTextures()
{
	auto& crate = m_textures.emplace( "crate", StaticTexture() ).first->second;
	std::unique_ptr<uint8_t[]> dds_data;
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	ThrowIfFailed( DirectX::LoadDDSTextureFromFile( md3dDevice.Get(), L"resources/textures/WoodCrate01.dds", crate.texture_gpu.GetAddressOf(), dds_data, subresources ) );

	size_t total_size = 0;
	for ( const auto& subresource : subresources )
		total_size += subresource.SlicePitch;

	// create upload buffer
	ThrowIfFailed( md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_UPLOAD ),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer( total_size ),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS( crate.texture_uploader.GetAddressOf() ) ) );

	// base resource is now in COPY_DEST STATE, no barrier required 
	UpdateSubresources( mCommandList.Get(), crate.texture_gpu.Get(), crate.texture_uploader.Get(), 0, 0, UINT( subresources.size() ), subresources.data() );
	mCommandList->ResourceBarrier( 1, &CD3DX12_RESOURCE_BARRIER::Transition( crate.texture_gpu.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ ) );
}

void RenderApp::BuildMaterials()
{
	{
		auto& grass_material = m_materials.emplace( "grass", StaticMaterial() ).first->second;
		auto& grass_data = grass_material.mat_constants;
		grass_data.mat_transform = MathHelper::Identity4x4();
		grass_data.diffuse_albedo = XMFLOAT4( 0.3f, 0.7f, 0.25f, 1.0f );
		grass_data.fresnel_r0 = XMFLOAT3( 0.6f, 0.7f, 0.5f );
		grass_data.roughness = 1.0f;
		grass_material.LoadToGPU( *md3dDevice.Get(), *mCommandList.Get() );
	}
	{
		auto& water_material = m_materials.emplace( "water", StaticMaterial() ).first->second;
		auto& water_data = water_material.mat_constants;
		water_data.mat_transform = MathHelper::Identity4x4();
		water_data.diffuse_albedo = XMFLOAT4( 0.0f, 0.2f, 0.6f, 1.0f );
		water_data.fresnel_r0 = XMFLOAT3( 0.1f, 0.1f, 0.1f );
		water_data.roughness = 0.0f;
		water_material.LoadToGPU( *md3dDevice.Get(), *mCommandList.Get() );
	}
}

void RenderApp::BuildLights()
{
	auto& parallel_light = m_lights.emplace( "sun", Light{ Light::Type::Parallel, {} } ).first->second.data;
	parallel_light.strength = XMFLOAT3( 1.0f, 1.0f, 1.0f );
	parallel_light.dir = XMFLOAT3( 0.2f, 0.7f, -0.3f );
	XMFloat3Normalize( parallel_light.dir );
}

void RenderApp::BuildRenderItems()
{
	auto init_from_submesh = []( const SubmeshGeometry& submesh, RenderItem& renderitem )
	{
		renderitem.n_frames_dirty = num_frame_resources;
		renderitem.index_count = submesh.IndexCount;
		renderitem.index_offset = submesh.StartIndexLocation;
		renderitem.vertex_offset = submesh.BaseVertexLocation;
	};

	{
		RenderItem land;
		land.cb_idx = 0;
		land.geom = &m_static_geometry["land_grid"];
		land.material = &m_materials["grass"];
		init_from_submesh( land.geom->DrawArgs["grid"], land );
		m_renderitems.push_back( land );
	}

	{
		RenderItem waves;
		waves.cb_idx = 1;
		waves.geom = &m_dynamic_geometry;
		waves.material = &m_materials["water"];
		init_from_submesh( waves.geom->DrawArgs["waves"], waves );
		m_renderitems.push_back( waves );
	}
}

template<DXGI_FORMAT index_format, class VCRange, class ICRange>
std::unordered_map<std::string, SubmeshGeometry>& RenderApp::LoadStaticGeometry( std::string name, const VCRange& vertices, const ICRange& indices, ID3D12GraphicsCommandList* cmd_list )
{
	_ASSERTE( cmd_list );

	using vertex_type = decltype( *vertices.data() );
	using index_type = decltype( *indices.data() );
	static_assert( index_format == DXGI_FORMAT_R16_UINT && sizeof( index_type ) == 2, "Wrong index type in LoadGeometry" ); // todo: enum -> sizeof map check

	const UINT vb_byte_size = UINT( vertices.size() ) * sizeof( vertex_type );
	const UINT ib_byte_size = UINT( indices.size() ) * sizeof( index_type );

	auto& new_geom = m_static_geometry.emplace( name, MeshGeometry() ).first->second;
	new_geom.Name = std::move( name );

	ThrowIfFailed( D3DCreateBlob( vb_byte_size, &new_geom.VertexBufferCPU ) );
	CopyMemory( new_geom.VertexBufferCPU->GetBufferPointer(), vertices.data(), vb_byte_size );

	ThrowIfFailed( D3DCreateBlob( ib_byte_size, &new_geom.IndexBufferCPU ) );
	CopyMemory( new_geom.IndexBufferCPU->GetBufferPointer(), indices.data(), ib_byte_size );

	new_geom.VertexBufferGPU = Utils::CreateDefaultBuffer( md3dDevice.Get(), cmd_list, vertices.data(), vb_byte_size, new_geom.VertexBufferUploader );
	new_geom.IndexBufferGPU = Utils::CreateDefaultBuffer( md3dDevice.Get(), cmd_list, indices.data(), ib_byte_size, new_geom.IndexBufferUploader );

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

	new_geom.IndexBufferGPU = Utils::CreateDefaultBuffer( md3dDevice.Get(), cmd_list, indices.data(), ib_byte_size, new_geom.IndexBufferUploader );

	new_geom.IndexFormat = index_format;
	new_geom.IndexBufferByteSize = ib_byte_size;
}

void RenderApp::BuildPSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc;
	ZeroMemory( &pso_desc, sizeof( D3D12_GRAPHICS_PIPELINE_STATE_DESC ) );

	pso_desc.InputLayout = { m_input_layout.data(), UINT( m_input_layout.size() ) };
	pso_desc.pRootSignature = m_root_signature.Get();
	pso_desc.VS =
	{
		reinterpret_cast<BYTE*>( m_vs_bytecode->GetBufferPointer() ),
		m_vs_bytecode->GetBufferSize()
	};

	pso_desc.PS =
	{
		reinterpret_cast<BYTE*>( m_ps_bytecode->GetBufferPointer() ),
		m_ps_bytecode->GetBufferSize()
	};

	pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC( D3D12_DEFAULT );
	pso_desc.BlendState = CD3DX12_BLEND_DESC( D3D12_DEFAULT );
	pso_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC( D3D12_DEFAULT );
	pso_desc.SampleMask = UINT_MAX;
	pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	pso_desc.NumRenderTargets = 1;
	pso_desc.RTVFormats[0] = mBackBufferFormat;
	pso_desc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	pso_desc.SampleDesc.Quality = m4xMsaaState ? ( m4xMsaaQuality - 1 ) : 0;

	pso_desc.DSVFormat = mDepthStencilFormat;

	ThrowIfFailed( md3dDevice->CreateGraphicsPipelineState( &pso_desc, IID_PPV_ARGS( &m_pso_main ) ) );

	pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	ThrowIfFailed( md3dDevice->CreateGraphicsPipelineState( &pso_desc, IID_PPV_ARGS( &m_pso_wireframe ) ) );
}

void RenderApp::BuildFrameResources()
{
	for ( int i = 0; i < num_frame_resources; ++i )
		m_frame_resources.emplace_back( md3dDevice.Get(), 1, UINT( m_renderitems.size() ), UINT( m_waves_cpu_vertices.size() ) );
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
