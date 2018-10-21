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

	cmd_list.SetGraphicsRootConstantBufferView( 2, context.pass_cbv );

	for ( const auto& render_item : *context.renderitems )
	{
		cmd_list.SetGraphicsRootConstantBufferView( 0, render_item.tf_addr );
		cmd_list.SetGraphicsRootDescriptorTable( 1, render_item.material->base_color_desc );

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
		 1 - base color map
		 2 - cbv per pass

		 Shader register bindings
		 b0 - cbv per obj
		 b1 - cbv per pass

		 t0 - base color
		 s0 - sampler
	*/

	constexpr int nparams = 3;

	CD3DX12_ROOT_PARAMETER slot_root_parameter[nparams];

	for ( int i = 0; i < 2; ++i )
		slot_root_parameter[i * 2].InitAsConstantBufferView( i );

	CD3DX12_DESCRIPTOR_RANGE desc_table;
	desc_table.Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0 );
	slot_root_parameter[1].InitAsDescriptorTable( 1, &desc_table );

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		0, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8 );                               // maxAnisotropy

	CD3DX12_ROOT_SIGNATURE_DESC root_sig_desc( nparams, slot_root_parameter,
											   1, &anisotropicWrap,
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

void DepthOnlyPass::BuildData( DXGI_FORMAT dsv_format, int bias, bool back_culling, ID3D12Device& device, ComPtr<ID3D12PipelineState>& pso, ComPtr<ID3D12RootSignature>& rootsig )
{
	const ::InputLayout input_layout = ForwardLightingPass::InputLayout();

	const auto shaders = LoadAndCompileShaders();
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
			reinterpret_cast<BYTE*>( shaders.first->GetBufferPointer() ),
			shaders.first->GetBufferSize()
		};

		pso_desc.PS =
		{
			reinterpret_cast<BYTE*>( shaders.second->GetBufferPointer() ),
			shaders.second->GetBufferSize()
		};

		pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC( D3D12_DEFAULT );
		if ( bias > 0 )
		{
			pso_desc.RasterizerState.DepthBias = bias;
			pso_desc.RasterizerState.SlopeScaledDepthBias = 1.0f;
		}
		pso_desc.RasterizerState.CullMode = back_culling ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_FRONT;
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

DepthOnlyPass::Shaders DepthOnlyPass::LoadAndCompileShaders()
{
	return { Utils::LoadBinary( L"shaders/depth_only_vs.cso" ), Utils::LoadBinary( L"shaders/depth_prepass_ps.cso" ) };
}
