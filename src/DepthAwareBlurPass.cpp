// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "stdafx.h"

#include "DepthAwareBlurPass.h"


DepthAwareBlurPass::DepthAwareBlurPass( ID3D12PipelineState* pso, ID3D12RootSignature* rootsig )
	: m_pso( pso ), m_root_signature( rootsig )
{
}


void DepthAwareBlurPass::Draw( const Context& context, ID3D12GraphicsCommandList& cmd_list )
{
	cmd_list.SetPipelineState( m_pso );
	cmd_list.SetComputeRootSignature( m_root_signature );

	cmd_list.SetComputeRootDescriptorTable( 0, context.input_srv );
	cmd_list.SetComputeRootDescriptorTable( 1, context.depth_srv );
	cmd_list.SetComputeRootDescriptorTable( 2, context.blurred_uav );
	cmd_list.SetComputeRootConstantBufferView( 3, context.pass_cb );

	const UINT group_cnt_x = ceil_integer_div( context.uav_width, GroupSizeX );
	const UINT group_cnt_y = ceil_integer_div( context.uav_height, GroupSizeY );
	cmd_list.Dispatch( group_cnt_x, group_cnt_y, 1 );
}


ComPtr<ID3D12RootSignature> DepthAwareBlurPass::BuildRootSignature( ID3D12Device& device )
{
	/*
	Blur pass root sig
	0 - source texture SRV
	1 - z-buffer
	2 - output texture UAV
	3 - pass cbv

	Shader register bindings
	t0 - source
	t1 - zbuffer
	u0 - output
	s0 - linear sampler
	*/

	constexpr int nparams = 4;

	CD3DX12_ROOT_PARAMETER slot_root_parameter[nparams];

	CD3DX12_DESCRIPTOR_RANGE desc_table[3];
	desc_table[0].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0 );
	desc_table[1].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1 );
	desc_table[2].Init( D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0 );
	slot_root_parameter[0].InitAsDescriptorTable( 1, desc_table );
	slot_root_parameter[1].InitAsDescriptorTable( 1, desc_table + 1 );
	slot_root_parameter[2].InitAsDescriptorTable( 1, desc_table + 2 );
	slot_root_parameter[3].InitAsConstantBufferView( 0 );

	CD3DX12_STATIC_SAMPLER_DESC input_sampler( 0, D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT );

	CD3DX12_ROOT_SIGNATURE_DESC root_sig_desc( nparams, slot_root_parameter, 1, &input_sampler );

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


void DepthAwareBlurPass::BuildData( ID3D12Device & device, ComPtr<ID3D12PipelineState>& pso, ComPtr<ID3D12RootSignature>& rootsig )
{
	if ( ! rootsig )
		rootsig = BuildRootSignature( device );

	if ( ! pso )
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc;
		ZeroMemory( &pso_desc, sizeof( D3D12_COMPUTE_PIPELINE_STATE_DESC ) );
		pso_desc.pRootSignature = rootsig.Get();

		const auto shader = LoadAndCompileShader();
		pso_desc.CS =
		{
			reinterpret_cast<BYTE*>( shader->GetBufferPointer() ),
			shader->GetBufferSize()
		};

		ThrowIfFailed( device.CreateComputePipelineState( &pso_desc, IID_PPV_ARGS( &pso ) ) );
	}
}


ComPtr<ID3DBlob> DepthAwareBlurPass::LoadAndCompileShader()
{
	return Utils::LoadBinary( L"shaders/separable_blur.cso" );
}
