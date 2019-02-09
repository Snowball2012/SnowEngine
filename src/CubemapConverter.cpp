// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "CubemapConverter.h"
#include "RenderUtils.h"

#include <dxtk12/CommonStates.h>
#include <dxtk12/DirectXHelpers.h>

using namespace DirectX;

CubemapConverter::CubemapConverter( ComPtr<ID3D12Device> device )
	: m_device( std::move( device ) )
{
	Init();
}


ComPtr<ID3D12Resource> CubemapConverter::MakeCubemapFromCylindrical( uint32_t resolution, D3D12_GPU_DESCRIPTOR_HANDLE texture_srv,
																	 DXGI_FORMAT cubemap_format, ID3D12GraphicsCommandList& cmd_list )
{
	assert( resolution > 0 && texture_srv.ptr != 0 );

	auto pso_it = m_pso.find( cubemap_format );
	if ( pso_it == m_pso.end() )
		pso_it = m_pso.emplace( cubemap_format, BuildPSO( cubemap_format, m_rs.Get() ) ).first;


	D3D12_RESOURCE_DESC cubemap_desc = GetCubemapDesc( resolution, cubemap_format );

	ComPtr<ID3D12Resource> cubemap;
	ThrowIfFailed( m_device->CreateCommittedResource( &CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT ),
													  D3D12_HEAP_FLAG_NONE,
													  &cubemap_desc,
													  D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr,
													  IID_PPV_ARGS( cubemap.GetAddressOf() ) ) );

	D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
	heap_desc.NumDescriptors = 6;
	heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	ThrowIfFailed( m_device->CreateDescriptorHeap( &heap_desc, IID_PPV_ARGS( cubemap_rtv_heap.GetAddressOf() ) ) );

	const UINT desc_handle_size = m_device->GetDescriptorHandleIncrementSize( heap_desc.Type );

	for ( int side = 0; side < 6; ++side )
	{
		D3D12_RENDER_TARGET_VIEW_DESC rtv_desc;
		rtv_desc.Format = cubemap_format;
		rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtv_desc.Texture2DArray.ArraySize = 1;
		rtv_desc.Texture2DArray.FirstArraySlice = side;
		rtv_desc.Texture2DArray.MipSlice = 0;
		rtv_desc.Texture2DArray.PlaneSlice = 0;
		m_device->CreateRenderTargetView( cubemap.Get(), &rtv_desc,
										  CD3DX12_CPU_DESCRIPTOR_HANDLE( cubemap_rtv_heap->GetCPUDescriptorHandleForHeapStart(), side * desc_handle_size ) );
	}

	cmd_list.SetPipelineState( pso_it->second.Get() );
	cmd_list.SetGraphicsRootSignature( m_rs.Get() );

	D3D12_VIEWPORT viewport;
	{
		viewport.TopLeftX = viewport.TopLeftY = viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		viewport.Height = viewport.Width = resolution;
	}
	D3D12_RECT scissor_rect;
	{
		scissor_rect.left = scissor_rect.top = 0;
		scissor_rect.right = scissor_rect.bottom = resolution;
	}
	cmd_list.RSSetViewports( 1, &viewport );
	cmd_list.RSSetScissorRects( 1, &scissor_rect );

	cmd_list.SetGraphicsRootDescriptorTable( 1, texture_srv );
	cmd_list.IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
	cmd_list.IASetIndexBuffer( nullptr );
	cmd_list.IASetVertexBuffers( 0, 0, nullptr );

	constexpr UINT64 cb_size = Utils::CalcConstantBufferByteSize( sizeof( XMFLOAT4X4 ) );

	for ( int side = 0; side < 6; ++side )
	{
		cmd_list.OMSetRenderTargets( 1, &CD3DX12_CPU_DESCRIPTOR_HANDLE( cubemap_rtv_heap->GetCPUDescriptorHandleForHeapStart(), side * desc_handle_size ),
									 true, nullptr );

		cmd_list.SetGraphicsRootConstantBufferView( 0, m_gpu_matrices->GetGPUVirtualAddress() + cb_size * side );

		cmd_list.DrawInstanced( 3, 1, 0, 0 );
	}

	return std::move( cubemap );
}


void CubemapConverter::Init()
{
	m_rs = BuildRootSignature();
	m_gpu_matrices = BuildGPUMatrices();
	m_shaders = CompileShaders();
}


ComPtr<ID3D12RootSignature> CubemapConverter::BuildRootSignature() const
{
	/*
	Cubemap generation pass root sig
	0 - inverse viewproj cb
	1 - texture srv

	Shader register bindings
	b0 - pass cb
	t0 - skybox srv
	s0 - wrap sampler
	*/

	constexpr int nparams = 2;

	CD3DX12_ROOT_PARAMETER1 slot_root_parameter[nparams];

	slot_root_parameter[0].InitAsConstantBufferView( 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE, D3D12_SHADER_VISIBILITY_PIXEL );
	CD3DX12_DESCRIPTOR_RANGE1 desc_table[1];
	desc_table[0].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0 );
	slot_root_parameter[1].InitAsDescriptorTable( 1, desc_table, D3D12_SHADER_VISIBILITY_PIXEL );

	CD3DX12_STATIC_SAMPLER_DESC linear_wrap_sampler( 0, D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT );
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_sig_desc( nparams, slot_root_parameter,
														 1, &linear_wrap_sampler );

	ComPtr<ID3DBlob> serialized_root_sig = nullptr;
	ComPtr<ID3DBlob> error_blob = nullptr;
	HRESULT hr = D3DX12SerializeVersionedRootSignature( &root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1_1,
														serialized_root_sig.GetAddressOf(), error_blob.GetAddressOf() );

	if ( error_blob )
	{
		OutputDebugStringA( (char*)error_blob->GetBufferPointer() );
	}
	ThrowIfFailed( hr );

	ComPtr<ID3D12RootSignature> rootsig;

	ThrowIfFailed( m_device->CreateRootSignature(
		0,
		serialized_root_sig->GetBufferPointer(),
		serialized_root_sig->GetBufferSize(),
		IID_PPV_ARGS( &rootsig ) ) );

	return std::move( rootsig );
}


CubemapConverter::Shaders CubemapConverter::CompileShaders()
{
	return Shaders{ Utils::LoadBinary( L"shaders/cubemap_gen_vs.cso" ), Utils::LoadBinary( L"shaders/cubemap_gen_ps.cso" ) };
}


ComPtr<ID3D12PipelineState> CubemapConverter::BuildPSO( DXGI_FORMAT rtv_format, ID3D12RootSignature* rs ) const
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};

	desc.DepthStencilState = CommonStates::DepthNone;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = rtv_format;
	desc.pRootSignature = rs;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.RasterizerState = CommonStates::CullNone;
	desc.BlendState = CommonStates::Opaque;
	desc.VS = D3D12_SHADER_BYTECODE{ m_shaders.vs->GetBufferPointer(), m_shaders.vs->GetBufferSize() };
	desc.PS = D3D12_SHADER_BYTECODE{ m_shaders.ps->GetBufferPointer(), m_shaders.ps->GetBufferSize() };
	desc.SampleMask = UINT_MAX;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;

	ComPtr<ID3D12PipelineState> pso;
	ThrowIfFailed( m_device->CreateGraphicsPipelineState( &desc, IID_PPV_ARGS( pso.GetAddressOf() ) ) );

	return std::move( pso );
}


ComPtr<ID3D12Resource> CubemapConverter::BuildGPUMatrices() const
{

	ComPtr<ID3D12Resource> constant_buffer;
	constexpr UINT64 cb_size = Utils::CalcConstantBufferByteSize( sizeof( XMFLOAT4X4 ) );
	ThrowIfFailed( m_device->CreateCommittedResource( &CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_UPLOAD ),
													  D3D12_HEAP_FLAG_NONE,
													  &CD3DX12_RESOURCE_DESC::Buffer( 6 * cb_size ),
													  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
													  IID_PPV_ARGS( constant_buffer.GetAddressOf() ) ) );

	void* data;
	ThrowIfFailed( constant_buffer->Map( 0, nullptr, &data ) );

	if ( !data )
		throw SnowEngineException( "nullptr returned after ID3D12Resource::Map()" );

	const XMMATRIX matrices[] =
	{
		XMMatrixRotationY( -XM_PIDIV2 ),		// Look: +x Index: 0
		XMMatrixRotationY( XM_PIDIV2 ),	// Look: -x Index: 1
		XMMatrixRotationX( XM_PIDIV2 ),	// Look: +y Index: 2
		XMMatrixRotationX( -XM_PIDIV2 ),		// Look: -y Index: 3
		XMMatrixIdentity(),					// Look: +z Index: 4
		XMMatrixRotationY( XM_PI )			// Look: -z Index: 5
	};

	const XMMATRIX proj = XMMatrixPerspectiveFovLH( XM_PIDIV2, 1.0f, 0.1f, 10.f );

	for ( int side = 0; side < 6; ++side )
	{
		XMFLOAT4X4* dst_matrix = reinterpret_cast<XMFLOAT4X4*>( reinterpret_cast<char*>( data )
																+ side * cb_size );

		XMVECTOR tmp_det;

		XMStoreFloat4x4( dst_matrix, XMMatrixTranspose( XMMatrixInverse( &tmp_det, matrices[side] * proj ) ) );
	}

	constant_buffer->Unmap( 0, nullptr );

	return std::move( constant_buffer );
}


D3D12_RESOURCE_DESC CubemapConverter::GetCubemapDesc( uint32_t resolution, DXGI_FORMAT cubemap_format ) const
{
	D3D12_RESOURCE_DESC desc;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0;
	desc.Width = desc.Height = resolution;
	desc.DepthOrArraySize = 6;
	desc.MipLevels = 1;
	desc.Format = cubemap_format;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	return desc;
}
