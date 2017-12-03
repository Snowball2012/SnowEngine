#include "stdafx.h"

#include "RenderApp.h"

using namespace DirectX;

RenderApp::RenderApp( HINSTANCE hinstance ): D3DApp( hinstance )
{
	mMainWndCaption = L"Snow Engine";
	//Set4xMsaaState( true );
}

bool RenderApp::Initialize()
{
	if ( ! D3DApp::Initialize() )
		return false;

	// create box, load to gpu through upload heap to default heap
	ThrowIfFailed( mCommandList->Reset( mDirectCmdListAlloc.Get(), nullptr ) );

	BuildFrameResources();
	BuildDescriptorHeaps();
	BuildConstantBuffers();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildBoxGeometry();
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

namespace
{
	struct Vertex
	{
		XMFLOAT3 pos;
		XMFLOAT4 color;
	};
}

namespace
{
	XMFLOAT3 SphericalToCartesian( float radius, float phi, float theta )
	{
		XMFLOAT3 res;

		res.x = radius * sinf( phi ) * cosf( theta );
		res.z = radius * sinf( phi ) * sinf( theta );
		res.y = radius * cosf( phi );
		return res;
	}
}

void RenderApp::Update( const GameTimer& gt )
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

	// Update mvp matrix then update constant buffer on gpu
	const auto& cartesian = SphericalToCartesian( m_radius, m_phi, m_theta );

	XMVECTOR pos = XMVectorSet( cartesian.x, cartesian.y, cartesian.z, 1.0f );
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );

	XMMATRIX view = XMMatrixLookAtLH( pos, target, up );
	XMStoreFloat4x4( &m_view, view );

	XMMATRIX world = XMLoadFloat4x4( &m_world );
	XMMATRIX proj = XMLoadFloat4x4( &m_proj );
	const auto& mvp = world * view * proj;

	// do update
	ObjectConstants obj_constants;
	XMStoreFloat4x4( &obj_constants.model_view_proj, XMMatrixTranspose( mvp ) );
	m_cur_frame_resource->object_cb->CopyData( 0, obj_constants );
}


void RenderApp::Draw( const GameTimer& gt )
{
	// Reuse memory associated with command recording
	// We can only reset when the associated command lists have finished execution on GPU
	m_cur_frame_resource->fence = ++mCurrentFence;
	mCommandQueue->Signal( mFence.Get(), mCurrentFence );

	ThrowIfFailed( m_cur_frame_resource->cmd_list_alloc->Reset() );

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList. Reusing the command list reuses memory
	ThrowIfFailed( mCommandList->Reset( m_cur_frame_resource->cmd_list_alloc.Get(), m_pso.Get() ) );

	// state transition( for back buffer )
	mCommandList->ResourceBarrier( 1, &CD3DX12_RESOURCE_BARRIER::Transition( CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET ) );

	// viewport and scissor rect
	mCommandList->RSSetViewports( 1, &mScreenViewport );
	mCommandList->RSSetScissorRects( 1, &mScissorRect );

	// clear the back buffer and depth
	const float bgr_color[4] = { 0.8f, 0.83f, 0.9f };
	mCommandList->ClearRenderTargetView( CurrentBackBufferView(), bgr_color, 0, nullptr );
	mCommandList->ClearDepthStencilView( DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr );

	// set render target
	mCommandList->OMSetRenderTargets( 1, &CurrentBackBufferView(), true, &DepthStencilView() );

	// set resources
	ID3D12DescriptorHeap* descriptor_heaps[] = { m_cbv_heap.Get() };
	mCommandList->SetDescriptorHeaps( _countof( descriptor_heaps ), descriptor_heaps );

	mCommandList->SetGraphicsRootSignature( m_root_signature.Get() );

	mCommandList->IASetVertexBuffers( 0, 1, &m_geometry.cbegin()->second.VertexBufferView() );
	mCommandList->IASetIndexBuffer( &m_geometry.cbegin()->second.IndexBufferView() );
	mCommandList->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

	mCommandList->SetGraphicsRootDescriptorTable( 0, m_cbv_heap->GetGPUDescriptorHandleForHeapStart() );

	mCommandList->DrawIndexedInstanced( (m_geometry.begin()->second).DrawArgs["box"].IndexCount, 1, 0, 0, 0 );

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
		m_radius += dx - dy;

		// Restrict the radius.
		m_radius = MathHelper::Clamp( m_radius, 3.0f, 15.0f );
	}

	m_last_mouse_pos.x = x;
	m_last_mouse_pos.y = y;
}

void RenderApp::BuildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC cbv_heap_desc;
	cbv_heap_desc.NumDescriptors = 3;
	cbv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbv_heap_desc.NodeMask = 0;

	ThrowIfFailed( md3dDevice->CreateDescriptorHeap( &cbv_heap_desc, IID_PPV_ARGS( &m_cbv_heap ) ) );
}

void RenderApp::BuildConstantBuffers()
{
	for ( int i = 0; i < num_frame_resources; ++i )
	{
		m_frame_resources[i].object_cb = std::make_unique<UploadBuffer<ObjectConstants>>( md3dDevice.Get(), 1, true );

		UINT obj_cb_bytesize = d3dUtil::CalcConstantBufferByteSize( sizeof( ObjectConstants ) );

		D3D12_GPU_VIRTUAL_ADDRESS cb_address = m_frame_resources[i].object_cb->Resource()->GetGPUVirtualAddress();

		int box_cbuf_index = 0;
		cb_address += box_cbuf_index * obj_cb_bytesize;

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc;
		{
			cbv_desc.BufferLocation = cb_address;
			cbv_desc.SizeInBytes = obj_cb_bytesize;
		}
		CD3DX12_CPU_DESCRIPTOR_HANDLE cbv_handle( m_cbv_heap->GetCPUDescriptorHandleForHeapStart(), i * md3dDevice->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ) );
		md3dDevice->CreateConstantBufferView( &cbv_desc, cbv_handle );
	}
}

void RenderApp::BuildRootSignature()
{
	CD3DX12_ROOT_PARAMETER slot_root_parameter[1];

	CD3DX12_DESCRIPTOR_RANGE cbv_table;
	cbv_table.Init( D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0 );
	slot_root_parameter[0].InitAsDescriptorTable( 1, &cbv_table );

	CD3DX12_ROOT_SIGNATURE_DESC root_sig_desc( 1, slot_root_parameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT );

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
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void RenderApp::BuildBoxGeometry()
{
	std::array<Vertex, 8> vertices =
	{
		Vertex( { XMFLOAT3( -1.0f, -1.0f, -1.0f ), XMFLOAT4( Colors::White ) } ),
		Vertex( { XMFLOAT3( -1.0f, +1.0f, -1.0f ), XMFLOAT4( Colors::Black ) } ),
		Vertex( { XMFLOAT3( +1.0f, +1.0f, -1.0f ), XMFLOAT4( Colors::Red ) } ),
		Vertex( { XMFLOAT3( +1.0f, -1.0f, -1.0f ), XMFLOAT4( Colors::Green ) } ),
		Vertex( { XMFLOAT3( -1.0f, -1.0f, +1.0f ), XMFLOAT4( Colors::Blue ) } ),
		Vertex( { XMFLOAT3( -1.0f, +1.0f, +1.0f ), XMFLOAT4( Colors::Yellow ) } ),
		Vertex( { XMFLOAT3( +1.0f, +1.0f, +1.0f ), XMFLOAT4( Colors::Cyan ) } ),
		Vertex( { XMFLOAT3( +1.0f, -1.0f, +1.0f ), XMFLOAT4( Colors::Magenta ) } )
	};

	std::array<std::uint16_t, 36> indices =
	{
		// front face
		0, 1, 2,
		0, 2, 3,

		// back face
		4, 6, 5,
		4, 7, 6,

		// left face
		4, 5, 1,
		4, 1, 0,

		// right face
		3, 2, 6,
		3, 6, 7,

		// top face
		1, 5, 6,
		1, 6, 2,

		// bottom face
		4, 0, 3,
		4, 3, 7
	};

	const UINT vb_byte_size = UINT( vertices.size() ) * sizeof( Vertex );
	const UINT ib_byte_size = UINT( indices.size() ) * sizeof( std::uint16_t );

	auto& box_geom = m_geometry.emplace( "box_geom", MeshGeometry() ).first->second;
	box_geom.Name = "box_geom";

	ThrowIfFailed( D3DCreateBlob( vb_byte_size, &box_geom.VertexBufferCPU ) );
	CopyMemory( box_geom.VertexBufferCPU->GetBufferPointer(), vertices.data(), vb_byte_size );

	ThrowIfFailed( D3DCreateBlob( ib_byte_size, &box_geom.IndexBufferCPU ) );
	CopyMemory( box_geom.IndexBufferCPU->GetBufferPointer(), indices.data(), ib_byte_size );

	box_geom.VertexBufferGPU = d3dUtil::CreateDefaultBuffer( md3dDevice.Get(), mCommandList.Get(), vertices.data(), vb_byte_size, box_geom.VertexBufferUploader );
	box_geom.IndexBufferGPU = d3dUtil::CreateDefaultBuffer( md3dDevice.Get(), mCommandList.Get(), indices.data(), ib_byte_size, box_geom.IndexBufferUploader );

	box_geom.VertexByteStride = sizeof( Vertex );
	box_geom.VertexBufferByteSize = vb_byte_size;
	box_geom.IndexFormat = DXGI_FORMAT_R16_UINT;
	box_geom.IndexBufferByteSize = ib_byte_size;

	SubmeshGeometry submesh;
	submesh.IndexCount = UINT( indices.size() );
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	box_geom.DrawArgs["box"] = submesh;
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

	ThrowIfFailed( md3dDevice->CreateGraphicsPipelineState( &pso_desc, IID_PPV_ARGS( &m_pso ) ) );
}

void RenderApp::BuildFrameResources()
{
	for ( int i = 0; i < num_frame_resources; ++i )
		m_frame_resources.emplace_back( md3dDevice.Get(), 1, 1 );
}
