#include "stdafx.h"

#include "RenderApp.h"

#include <WindowsX.h>

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <cetonia/parser.h>

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

	BuildFrameResources();
	BuildDescriptorHeaps();
	BuildConstantBuffers();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	/*{
		std::vector<ctTokenLine2d> new_lines;
		new_lines.push_back( { {0, 0}, {0, 1}, {0,0,0}, 0.1 } );
		std::vector<Vertex> vertices;
		vertices.resize( 4 * new_lines.size() );

		for ( int i = 0; i < new_lines.size(); ++i )
		{
			MakeLineVertices( new_lines[i], vertices.data() + i * 4 );
		}

		auto& submeshes = LoadGeometry<DXGI_FORMAT_R16_UINT>( std::to_string( m_cetonia_counter++ ), vertices, m_line_indices );
		for ( int i = 0; i < new_lines.size(); ++i )
		{
			SubmeshGeometry submesh;
			submesh.IndexCount = 6;
			submesh.StartIndexLocation = 0;
			submesh.BaseVertexLocation = i * 4;
			submeshes[std::to_string( i )] = submesh;
		}
	}*/
	//BuildGeometry();
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

	XMMATRIX proj = XMLoadFloat4x4( &m_proj );
	const auto& vp = view * proj;

	if ( ! m_new_lines.empty() )
	{
		ThrowIfFailed( mCommandList->Reset( mDirectCmdListAlloc.Get(), nullptr ) );

		std::vector<Vertex> vertices;
		vertices.resize( 4 * m_new_lines.size() );

		for ( int i = 0; i < m_new_lines.size(); ++i )
			MakeLineVertices( m_new_lines[i], vertices.data() + i * 4 );

		auto& submeshes = LoadGeometry<DXGI_FORMAT_R16_UINT>( std::to_string( m_cetonia_counter++ ), vertices, m_line_indices );
		for ( int i = 0; i < m_new_lines.size(); ++i )
		{
			SubmeshGeometry submesh;
			submesh.IndexCount = 6;
			submesh.StartIndexLocation = 0;
			submesh.BaseVertexLocation = i * 4;
			submeshes[std::to_string( i )] = submesh;
		}
		m_new_lines.clear();

		ThrowIfFailed( mCommandList->Close() );
		ID3D12CommandList* cmd_lists[] = { mCommandList.Get() };
		mCommandQueue->ExecuteCommandLists( _countof( cmd_lists ), cmd_lists );

		FlushCommandQueue();
	}
	// do update
	ObjectConstants obj_constants;
	XMStoreFloat4x4( &obj_constants.model_view_proj, XMMatrixTranspose( vp ) );
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

	mCommandList->SetGraphicsRootDescriptorTable( 0, m_cbv_heap->GetGPUDescriptorHandleForHeapStart() );

	for ( const auto& geom_item : m_geometry )
	{
		mCommandList->IASetVertexBuffers( 0, 1, &geom_item.second.VertexBufferView() );
		mCommandList->IASetIndexBuffer( &geom_item.second.IndexBufferView() );
		mCommandList->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
		for ( const auto& submesh : geom_item.second.DrawArgs )
			mCommandList->DrawIndexedInstanced( submesh.second.IndexCount, 1, submesh.second.StartIndexLocation, submesh.second.BaseVertexLocation, 0 );
	}

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
		m_radius += (dx - dy) * m_radius;

		// Restrict the radius.
		m_radius = MathHelper::Clamp( m_radius, 3.0f, 1000.0f );
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

void RenderApp::BuildGeometry()
{
	if ( strlen( m_cmd_line ) == 0 )
	{
		// box and pyramid, 2 submeshes
		std::array<Vertex, 8 + 5> vertices =
		{
			// box
			Vertex( { XMFLOAT3( -1.0f, -1.0f, -1.0f ), XMFLOAT4( Colors::White ) } ),
			Vertex( { XMFLOAT3( -1.0f, +1.0f, -1.0f ), XMFLOAT4( Colors::Black ) } ),
			Vertex( { XMFLOAT3( +1.0f, +1.0f, -1.0f ), XMFLOAT4( Colors::Red ) } ),
			Vertex( { XMFLOAT3( +1.0f, -1.0f, -1.0f ), XMFLOAT4( Colors::Green ) } ),
			Vertex( { XMFLOAT3( -1.0f, -1.0f, +1.0f ), XMFLOAT4( Colors::Blue ) } ),
			Vertex( { XMFLOAT3( -1.0f, +1.0f, +1.0f ), XMFLOAT4( Colors::Yellow ) } ),
			Vertex( { XMFLOAT3( +1.0f, +1.0f, +1.0f ), XMFLOAT4( Colors::Cyan ) } ),
			Vertex( { XMFLOAT3( +1.0f, -1.0f, +1.0f ), XMFLOAT4( Colors::Magenta ) } ),

			// pyramid
			Vertex( { XMFLOAT3( -1.0f, 2.0f, -1.0f ), XMFLOAT4( Colors::White ) } ),
			Vertex( { XMFLOAT3( -1.0f, 2.0f, +1.0f ), XMFLOAT4( Colors::Black ) } ),
			Vertex( { XMFLOAT3( +1.0f, 2.0f, +1.0f ), XMFLOAT4( Colors::Red ) } ),
			Vertex( { XMFLOAT3( +1.0f, 2.0f, -1.0f ), XMFLOAT4( Colors::Green ) } ),
			Vertex( { XMFLOAT3( +0.0f, 3.0f, +0.0f ), XMFLOAT4( Colors::Blue ) } ),
		};

		std::array<std::uint16_t, 36 + 18> indices =
		{
			// box

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
			4, 3, 7,

			// pyramid
			8, 9, 12,
			9, 10, 12,
			10, 11, 12,
			11, 8, 12,
			10, 9, 8,
			8, 11, 10
		};

		auto& submeshes = LoadGeometry<DXGI_FORMAT_R16_UINT>( "box_geom", vertices, indices );

		SubmeshGeometry submesh;
		submesh.IndexCount = 36;
		submesh.StartIndexLocation = 0;
		submesh.BaseVertexLocation = 0;

		submeshes["box"] = submesh;

		submesh.IndexCount = 18;
		submesh.StartIndexLocation = 36;
		submesh.BaseVertexLocation = 0;

		submeshes["pyramid"] = submesh;
	}
	else
	{
		std::ifstream file;
		file.open( m_cmd_line );
		if ( ! file.is_open() )
			throw SnowEngineException( "can't read .obj" );
		MeshData mesh_data = ObjImporter().ParseObj( file );

		auto& submeshes = LoadGeometry<DXGI_FORMAT_R16_UINT>( "main", mesh_data.m_vertices, mesh_data.m_faces );

		SubmeshGeometry submesh;
		submesh.IndexCount = UINT( mesh_data.m_faces.size() );
		submesh.StartIndexLocation = 0;
		submesh.BaseVertexLocation = 0;

		submeshes["main_sub"] = submesh;
	}
}

template<DXGI_FORMAT index_format, class VCRange, class ICRange>
std::unordered_map<std::string, SubmeshGeometry>& RenderApp::LoadGeometry( std::string name, const VCRange& vertices, const ICRange& indices )
{
	using vertex_type = decltype( *vertices.data() );
	using index_type = decltype( *indices.data() );
	static_assert( index_format == DXGI_FORMAT_R16_UINT && sizeof( index_type ) == 2, "Wrong index type in LoadGeometry" ); // todo: enum -> sizeof map check

	const UINT vb_byte_size = UINT( vertices.size() ) * sizeof( vertex_type );
	const UINT ib_byte_size = UINT( indices.size() ) * sizeof( index_type );

	auto& new_geom = m_geometry.emplace( name, MeshGeometry() ).first->second;
	new_geom.Name = std::move( name );

	ThrowIfFailed( D3DCreateBlob( vb_byte_size, &new_geom.VertexBufferCPU ) );
	CopyMemory( new_geom.VertexBufferCPU->GetBufferPointer(), vertices.data(), vb_byte_size );

	ThrowIfFailed( D3DCreateBlob( ib_byte_size, &new_geom.IndexBufferCPU ) );
	CopyMemory( new_geom.IndexBufferCPU->GetBufferPointer(), indices.data(), ib_byte_size );

	new_geom.VertexBufferGPU = d3dUtil::CreateDefaultBuffer( md3dDevice.Get(), mCommandList.Get(), vertices.data(), vb_byte_size, new_geom.VertexBufferUploader );
	new_geom.IndexBufferGPU = d3dUtil::CreateDefaultBuffer( md3dDevice.Get(), mCommandList.Get(), indices.data(), ib_byte_size, new_geom.IndexBufferUploader );

	new_geom.VertexByteStride = sizeof( vertex_type );
	new_geom.VertexBufferByteSize = vb_byte_size;
	new_geom.IndexFormat = index_format;
	new_geom.IndexBufferByteSize = ib_byte_size;

	return new_geom.DrawArgs;
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

namespace
{
	// temporary
	XMFLOAT3 operator-( const XMFLOAT3& lhs, const XMFLOAT3& rhs )
	{
		return XMFLOAT3( lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z );
	}

	XMFLOAT3 operator+( const XMFLOAT3& lhs, const XMFLOAT3& rhs )
	{
		return XMFLOAT3( lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z );
	}

	XMFLOAT3 operator*( const XMFLOAT3& lhs, float mod )
	{
		return XMFLOAT3( lhs.x * mod, lhs.y * mod, lhs.z * mod );
	}

	XMFLOAT3 operator*( float mod, const XMFLOAT3& op )
	{
		return op*mod;
	}

	XMFLOAT3& operator*=( XMFLOAT3& op, float mod )
	{
		op = mod * op;
		return op;
	}

	XMFLOAT3 operator/( const XMFLOAT3& lhs, float mod )
	{
		return XMFLOAT3( lhs.x / mod, lhs.y / mod, lhs.z / mod );
	}

	float XMFloat3LenSquared( const XMFLOAT3& op )
	{
		return op.x * op.x + op.y * op.y + op.z * op.z;
	}

	float XMFloat3Normalize( XMFLOAT3& lhs )
	{
		float res = sqrt( XMFloat3LenSquared( lhs ) );
		lhs = lhs / res;
		return res;
	}
}

void RenderApp::MakeLineVertices( const ctTokenLine2d& line, Vertex vertices[4] ) const
{
	XMFLOAT3 p1( float(line.p1.x), float(line.p1.y), 0.f );
	XMFLOAT3 p2( float(line.p2.x), float(line.p2.y), 0.f );

	XMFLOAT3 left_dir = p2 - p1;
	std::swap( left_dir.x, left_dir.y );
	left_dir.x *= -1.f;
	XMFloat3Normalize( left_dir );

	left_dir *= float(line.thickness);

	for ( int i : { 0, 1 } )
	{
		vertices[i].pos = p1 + ( i * 2 - 1 ) * left_dir;
		vertices[i + 2].pos = p2 + ( i * 2 - 1 ) * left_dir;
	}

	for ( int i = 0; i < 4; ++i )
		vertices[i].color = XMFLOAT4( float(line.color.r), float(line.color.g), float(line.color.b), 1.f );
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

using namespace boost::interprocess;
void RenderApp::OnKeyUp( WPARAM btn )
{
	// entry point for cetonia
	if ( (int)btn == VK_F8 )
	{
		std::vector<ctTokenLine2d> new_lines;
		try
		{
			struct remover
			{
				~remover()
				{
					shared_memory_object::remove( "cetonia" );
				}
			} onremove;
			shared_memory_object shm( open_or_create, "cetonia", read_only );

			boost::interprocess::offset_t shm_size;
			if ( ! shm.get_size( shm_size ) )
				return;

			if ( shm_size == 0 )
				return;

			mapped_region region( shm, read_only, 0, shm_size );
			if ( region.get_size() == 0 )
				return;

			size_t msg_size = *reinterpret_cast<const size_t*>( region.get_address() );

			const char* msg = reinterpret_cast<const char*>( region.get_address() ) + sizeof( size_t );

			size_t stride = 0;
			while ( stride < msg_size )
			{
				size_t token_shift;
				ctToken token;
				if ( ctFailed( ctParseToken( msg + stride, msg_size - stride, &token, &token_shift ) ) || ! ctIsTokenValid( token.type ) )
					throw SnowEngineException( "fail in ctParseToken" );

				stride += token_shift;

				switch ( token.type )
				{
					case CT_Line2d:
					{
						new_lines.emplace_back( token.data.l2d );
						break;
					}
				}
			}
		}
		catch ( ... )
		{
			throw SnowEngineException( "fail in cetonia" );
		}

		if ( ! new_lines.empty() )
			std::swap( m_new_lines, new_lines );
	}
}
