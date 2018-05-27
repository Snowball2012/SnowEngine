#pragma once

#include "RenderPass.h"

#include "RenderData.h"

class ForwardLightingPass
{
public:
	ForwardLightingPass( ID3D12PipelineState* pso, ID3D12PipelineState* wireframe_pso, ID3D12RootSignature* rootsig )
		: m_pso( pso ), m_pso_wireframe( wireframe_pso ), m_root_signature( rootsig ) {}

	struct ForwardLightingContext
	{
		RenderSceneContext* scene;
		const D3D12_CPU_DESCRIPTOR_HANDLE* back_buffer_rtv;
		const D3D12_CPU_DESCRIPTOR_HANDLE* depth_stencil_view;
		ID3D12Resource* pass_cb;
		ID3D12Resource* object_cb;
	};

	// all descriptor heaps must be set prematurely
	void Draw( const ForwardLightingContext& context, bool wireframe, ID3D12GraphicsCommandList& cmd_list );

	template<class StaticSamplerRange>
	static void BuildRootSignature( const StaticSamplerRange& static_samplers, ComPtr<ID3D12RootSignature>& rootsig );

	static void BuildPSOs( ID3D12Device& device, ComPtr<ID3D12PipelineState>& main_pso, ComPtr<ID3D12PipelineState>& wireframe_pso );

	static inline InputLayout InputLayout();

private:
	ID3D12PipelineState* m_pso = nullptr;
	ID3D12PipelineState* m_pso_wireframe = nullptr;
	ID3D12RootSignature* m_root_signature = nullptr;
};


// template implementation
template<class StaticSamplerRange>
inline void ForwardLightingPass::BuildRootSignature( const StaticSamplerRange& static_samplers, ComPtr<ID3D12RootSignature>& rootsig )
{
	/*
	Basic root sig
	0 - cbv per object
	1 - cbv per material
	2 - albedo
	3 - normal
	4 - specular
	5 - cbv per pass

	Shader register bindings
	b0 - cbv per obj
	b1 - cbv per material
	b2 - cbv per pass

	t0 - albedo
	t1 - normal
	t2 - specular

	To optimize state changes keep material textures together when possible. Descriptor duplication?
	*/
	constexpr int nparams = 6;

	CD3DX12_ROOT_PARAMETER slot_root_parameter[nparams];

	for ( int i = 0; i < 2; ++i )
		slot_root_parameter[i].InitAsConstantBufferView( i );

	CD3DX12_DESCRIPTOR_RANGE desc_table[3];
	desc_table[0].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0 ); // albedo
	desc_table[1].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1 ); // normal
	desc_table[2].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2 ); // specular

	slot_root_parameter[2].InitAsDescriptorTable( 1, &desc_table[0], D3D12_SHADER_VISIBILITY_ALL );
	slot_root_parameter[3].InitAsDescriptorTable( 1, &desc_table[1], D3D12_SHADER_VISIBILITY_ALL );
	slot_root_parameter[4].InitAsDescriptorTable( 1, &desc_table[2], D3D12_SHADER_VISIBILITY_ALL );

	slot_root_parameter[5].InitAsConstantBufferView( 2 );

	const auto& static_samplers = BuildStaticSamplers();
	CD3DX12_ROOT_SIGNATURE_DESC root_sig_desc( nparams, slot_root_parameter, UINT( static_samplers.size() ), static_samplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT );

	ComPtr<ID3DBlob> serialized_root_sig = nullptr;
	ComPtr<ID3DBlob> error_blob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature( &root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1, serialized_root_sig.GetAddressOf(), error_blob.GetAddressOf() );

	if ( error_blob )
	{
		OutputDebugStringA( (char*)error_blob->GetBufferPointer() );
	}
	ThrowIfFailed( hr );

	ThrowIfFailed( m_d3d_device->CreateRootSignature(
		0,
		serialized_root_sig->GetBufferPointer(),
		serialized_root_sig->GetBufferSize(),
		IID_PPV_ARGS( &rootsig ) ) );
}

inline void ForwardLightingPass::BuildPSOs( ID3D12Device& device, ComPtr<ID3D12PipelineState>& main_pso, ComPtr<ID3D12PipelineState>& wireframe_pso )
{
	NOTIMPL;
}

inline InputLayout ForwardLightingPass::InputLayout()
{
	return
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}