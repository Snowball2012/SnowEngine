#include "stdafx.h"

#include "DepthOnlyPass.h"

#include "ForwardLightingPass.h"


DepthOnlyPass::DepthOnlyPass( ID3D12PipelineState* pso, ID3D12RootSignature* rootsig )
	: m_pso( pso ), m_root_signature( rootsig )
{
}

void DepthOnlyPass::Draw( const Context& context, ID3D12GraphicsCommandList& cmd_list )
{
	cmd_list.SetPipelineState( m_pso );

	cmd_list.OMSetRenderTargets( 0, nullptr, false, &context.depth_stencil_view );

	cmd_list.SetGraphicsRootSignature( m_root_signature );

	const auto pass_cb_address = context.pass_cb->GetGPUVirtualAddress();
	const auto pass_cb_size = Utils::CalcConstantBufferByteSize( sizeof( PassConstants ) );
	cmd_list.SetGraphicsRootConstantBufferView( 1, pass_cb_address + pass_cb_size * context.pass_cb_idx );

	const auto obj_cb_address = context.object_cb->GetGPUVirtualAddress();
	const auto obj_cb_size = Utils::CalcConstantBufferByteSize( sizeof( ObjectConstants ) );
	for ( const auto& render_item : context.scene->renderitems )
	{
		cmd_list.SetGraphicsRootConstantBufferView( 0, obj_cb_address + render_item.cb_idx * obj_cb_size );

		cmd_list.IASetVertexBuffers( 0, 1, &render_item.geom->VertexBufferView() );
		cmd_list.IASetIndexBuffer( &render_item.geom->IndexBufferView() );
		cmd_list.IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
		cmd_list.DrawIndexedInstanced( render_item.index_count, 1, render_item.index_offset, render_item.vertex_offset, 0 );
	}
}

ComPtr<ID3D12RootSignature> DepthOnlyPass::BuildRootSignature( ID3D12Device& device )
{
	/*
		Depth pass root sig
		 0 - cbv per object
		 1 - cbv per pass

		 Shader register bindings
		 b0 - cbv per obj
		 b1 - cbv per pass
	*/

	constexpr int nparams = 2;

	CD3DX12_ROOT_PARAMETER slot_root_parameter[nparams];

	for ( int i = 0; i < 2; ++i )
		slot_root_parameter[i].InitAsConstantBufferView( i );

	CD3DX12_ROOT_SIGNATURE_DESC root_sig_desc( nparams, slot_root_parameter,
											   0, nullptr,
											   D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT );

	ComPtr<ID3DBlob> serialized_root_sig = nullptr;
	ComPtr<ID3DBlob> error_blob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature( &root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1,
											  serialized_root_sig.GetAddressOf(), error_blob.GetAddressOf() );

	if ( error_blob )
	{
		OutputDebugStringA( (char*)error_blob->GetBufferPointer() );
	}
	ThrowIfFailed( hr );

	ComPtr<ID3D12RootSignature> rootsig;

	ThrowIfFailed( device.CreateRootSignature(
		0,
		serialized_root_sig->GetBufferPointer(),
		serialized_root_sig->GetBufferSize(),
		IID_PPV_ARGS( &rootsig ) ) );

	return rootsig;
}

void DepthOnlyPass::BuildData( DXGI_FORMAT dsv_format, ID3D12Device& device, ComPtr<ID3D12PipelineState>& pso, ComPtr<ID3D12RootSignature>& rootsig )
{
	const ::InputLayout input_layout = ForwardLightingPass::InputLayout();

	const auto shader = LoadAndCompileVertexShader();
	if ( ! rootsig )
		rootsig = BuildRootSignature( device );

	if ( ! pso )
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc;
		ZeroMemory( &pso_desc, sizeof( D3D12_GRAPHICS_PIPELINE_STATE_DESC ) );

		pso_desc.InputLayout = { input_layout.data(), UINT( input_layout.size() ) };
		pso_desc.pRootSignature = rootsig.Get();

		pso_desc.VS =
		{
			reinterpret_cast<BYTE*>( shader->GetBufferPointer() ),
			shader->GetBufferSize()
		};

		pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC( D3D12_DEFAULT );
		pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
		pso_desc.BlendState = CD3DX12_BLEND_DESC( D3D12_DEFAULT );
		pso_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC( D3D12_DEFAULT );
		pso_desc.SampleMask = UINT_MAX;
		pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		pso_desc.NumRenderTargets = 0;
		pso_desc.SampleDesc.Count = 1;
		pso_desc.SampleDesc.Quality = 0;

		pso_desc.DSVFormat = dsv_format;

		ThrowIfFailed( device.CreateGraphicsPipelineState( &pso_desc, IID_PPV_ARGS( &pso ) ) );
	}
}

ComPtr<ID3DBlob> DepthOnlyPass::LoadAndCompileVertexShader()
{
	return Utils::LoadBinary( L"shaders/depth_only_vs.cso" );
}
