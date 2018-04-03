#include "stdafx.h"

#include "RenderApp.h"

#include "GeomGeneration.h"

#include <WindowsX.h>

#include <boost/range/adaptors.hpp>

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

	// create box, load to gpu through upload heap to default heap
	ThrowIfFailed( mCommandList->Reset( mDirectCmdListAlloc.Get(), nullptr ) );

	BuildGeometry();
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

	UpdateWaves( gt );
	UpdateDynamicGeometry( *m_cur_frame_resource->dynamic_geom_vb );

	// Update pass constants
	UpdatePassConstants( gt, *m_cur_frame_resource->pass_cb );

	// Update object constants if needed
	for ( auto& renderitem : m_renderitems )
		UpdateRenderItem( renderitem, *m_cur_frame_resource->object_cb );
}

void RenderApp::UpdatePassConstants( const GameTimer& gt, UploadBuffer<PassConstants>& pass_cb )
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

	pass_cb.CopyData( 0, pc );
}

void RenderApp::UpdateRenderItem( RenderItem& renderitem, UploadBuffer<ObjectConstants>& obj_cb )
{
	if ( renderitem.n_frames_dirty > 0 )
	{
		ObjectConstants obj_constants;
		XMStoreFloat4x4( &obj_constants.model, XMMatrixTranspose( XMLoadFloat4x4( &renderitem.world_mat ) ) );
		obj_cb.CopyData( renderitem.cb_idx, obj_constants );
		renderitem.n_frames_dirty--;
	}
}

void RenderApp::UpdateDynamicGeometry( UploadBuffer<Vertex>& dynamic_vb )
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
	cmd_list->SetGraphicsRootConstantBufferView( 1, m_cur_frame_resource->pass_cb->Resource()->GetGPUVirtualAddress() );

	const auto obj_cb_adress = m_cur_frame_resource->object_cb->Resource()->GetGPUVirtualAddress();
	const auto obj_cb_size = d3dUtil::CalcConstantBufferByteSize( sizeof( ObjectConstants ) );
	for ( const auto& render_item : m_renderitems )
	{
		cmd_list->SetGraphicsRootConstantBufferView( 0, obj_cb_adress + render_item.cb_idx * obj_cb_size );
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
	// no heaps =(
}

void RenderApp::BuildConstantBuffers()
{
	for ( int i = 0; i < num_frame_resources; ++i )
	{
		// object cb
		m_frame_resources[i].object_cb = std::make_unique<UploadBuffer<ObjectConstants>>( md3dDevice.Get(), UINT( m_renderitems.size() ), true );
		// pass cb
		m_frame_resources[i].pass_cb = std::make_unique<UploadBuffer<PassConstants>>( md3dDevice.Get(), num_passes, true );
	}
}

void RenderApp::BuildRootSignature()
{
	CD3DX12_ROOT_PARAMETER slot_root_parameter[2];

	slot_root_parameter[0].InitAsConstantBufferView( 0 );
	slot_root_parameter[1].InitAsConstantBufferView( 1 );

	CD3DX12_ROOT_SIGNATURE_DESC root_sig_desc( 2, slot_root_parameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT );

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
	m_vs_bytecode = d3dUtil::LoadBinary( L"shaders/vs.cso" );
	m_ps_bytecode = d3dUtil::LoadBinary( L"shaders/ps.cso" );

	m_input_layout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

namespace
{
	float hillsHeight( float x, float z )
	{
		return 0.3f*( z*sinf( 0.1f*x ) + x*cosf( 0.1f*z ) );
	}

	XMFLOAT4 colorByHeight( float height )
	{
		// Color the vertex based on its height.
		if ( height < -10.0f )
		{
			// Sandy beach color.
			return XMFLOAT4( 1.0f, 0.96f, 0.62f, 1.0f );
		}
		else if ( height < 5.0f )
		{
			// Light yellow-green.
			return  XMFLOAT4( 0.48f, 0.77f, 0.46f, 1.0f );
		}
		else if ( height < 12.0f )
		{
			// Dark yellow-green.
			return XMFLOAT4( 0.1f, 0.48f, 0.19f, 1.0f );
		}
		else if ( height < 20.0f )
		{
			// Dark brown.
			return XMFLOAT4( 0.45f, 0.39f, 0.34f, 1.0f );
		}
		else
		{
			// White snow.
			return XMFLOAT4( 1.0f, 1.0f, 1.0f, 1.0f );
		}
	}
}

void RenderApp::BuildGeometry()
{
	static constexpr size_t grid_nx = 50;
	static constexpr size_t grid_ny = 50;

	// static

	{
		auto grid = GeomGeneration::MakeArrayGrid<grid_nx, grid_ny>( 160, 160 );

		for ( auto& vertex : grid.first )
		{
			vertex.pos.y = hillsHeight( vertex.pos.x, vertex.pos.z );
			vertex.color = colorByHeight( vertex.pos.y );
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
			m_waves_cpu_vertices.emplace_back( Vertex{ DirectX::XMFLOAT3( x, 0, y ), DirectX::XMFLOAT4( 0.5f, 0.5f, 0.8f, 1.0f ) } );
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
		init_from_submesh( land.geom->DrawArgs["grid"], land );
		m_renderitems.push_back( land );
	}

	{
		RenderItem waves;
		waves.cb_idx = 1;
		waves.geom = &m_dynamic_geometry;
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

	new_geom.VertexBufferGPU = d3dUtil::CreateDefaultBuffer( md3dDevice.Get(), cmd_list, vertices.data(), vb_byte_size, new_geom.VertexBufferUploader );
	new_geom.IndexBufferGPU = d3dUtil::CreateDefaultBuffer( md3dDevice.Get(), cmd_list, indices.data(), ib_byte_size, new_geom.IndexBufferUploader );

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

	new_geom.IndexBufferGPU = d3dUtil::CreateDefaultBuffer( md3dDevice.Get(), cmd_list, indices.data(), ib_byte_size, new_geom.IndexBufferUploader );

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
		case WM_KEYUP:
		{
			if ( wParam == VK_ESCAPE )
			{
				PostQuitMessage( 0 );
			}
			else if ( (int)wParam == VK_F2 )
			{
				Set4xMsaaState( !m4xMsaaState );
			}
			else
			{
				OnKeyUp( wParam );
			}

			return 0;
		}
	}

	return D3DApp::MsgProc( hwnd, msg, wParam, lParam );
}

void RenderApp::OnKeyUp( WPARAM btn )
{
	// 'F2' - wireframe mode
	if ( (int)btn == VK_F3 )
		m_wireframe_mode = ! m_wireframe_mode;
}
