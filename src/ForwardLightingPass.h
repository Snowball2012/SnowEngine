#pragma once

#include "RenderData.h"

class ForwardLightingPass
{
public:
	ForwardLightingPass( ID3D12PipelineState* pso, ID3D12PipelineState* wireframe_pso, ID3D12RootSignature* rootsig );

	struct Context
	{
		RenderSceneContext* scene;
		D3D12_CPU_DESCRIPTOR_HANDLE back_buffer_rtv;
		D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil_view;
		D3D12_CPU_DESCRIPTOR_HANDLE ambient_rtv;
		D3D12_CPU_DESCRIPTOR_HANDLE normals_rtv;
		D3D12_GPU_DESCRIPTOR_HANDLE shadow_map_srv;
		D3D12_GPU_VIRTUAL_ADDRESS pass_cb;
	};

	// all descriptor heaps must be set prematurely
	void Draw( const Context& context, bool wireframe, ID3D12GraphicsCommandList& cmd_list );

	// static build methods
	template<class StaticSamplerRange>
	static ComPtr<ID3D12RootSignature> BuildRootSignature( const StaticSamplerRange& static_samplers, ID3D12Device& device );

	template<class StaticSamplerRange>
	static void BuildData( const StaticSamplerRange& static_samplers,
						   DXGI_FORMAT rendertarget_format, DXGI_FORMAT ambient_rtv_format, DXGI_FORMAT normal_format, DXGI_FORMAT depth_stencil_format,
						   ID3D12Device& device,
						   ComPtr<ID3D12PipelineState>& main_pso,
						   ComPtr<ID3D12PipelineState>& wireframe_pso,
						   ComPtr<ID3D12RootSignature>& rootsig );

	struct Shaders
	{
		ComPtr<ID3DBlob> vs;
		ComPtr<ID3DBlob> gs;
		ComPtr<ID3DBlob> ps;
	};
	static Shaders LoadAndCompileShaders();

	static inline InputLayout InputLayout();

private:
	ID3D12PipelineState* m_pso = nullptr;
	ID3D12PipelineState* m_pso_wireframe = nullptr;
	ID3D12RootSignature* m_root_signature = nullptr;
};


// template implementation
template<class StaticSamplerRange>
inline ComPtr<ID3D12RootSignature> ForwardLightingPass::BuildRootSignature( const StaticSamplerRange& static_samplers, 
																			ID3D12Device& device )
{
	/*
		Basic root sig
		0 - cbv per object
		1 - cbv per material
		2 - material textures descriptor table
		3 - shadow
		4 - cbv per pass

		Shader register bindings
		b0 - cbv per obj
		b1 - cbv per material
		b2 - cbv per pass

		t0 - albedo
		t1 - normal
		t2 - specular
		t3 - shadow

		To optimize state changes keep material textures together when possible. Descriptor duplication?
	*/
	constexpr int nparams = 5;

	CD3DX12_ROOT_PARAMETER slot_root_parameter[nparams];

	for ( int i = 0; i < 2; ++i )
		slot_root_parameter[i].InitAsConstantBufferView( i );

	CD3DX12_DESCRIPTOR_RANGE desc_table[2];
	desc_table[0].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0 ); // albedo, normal, specular
	desc_table[1].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3 ); // shadow

	slot_root_parameter[2].InitAsDescriptorTable( 1, &desc_table[0], D3D12_SHADER_VISIBILITY_ALL );
	slot_root_parameter[3].InitAsDescriptorTable( 1, &desc_table[1], D3D12_SHADER_VISIBILITY_ALL );

	slot_root_parameter[4].InitAsConstantBufferView( 2 );

	CD3DX12_ROOT_SIGNATURE_DESC root_sig_desc( nparams, slot_root_parameter,
											   UINT( static_samplers.size() ), static_samplers.data(),
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

template<class StaticSamplerRange>
inline void ForwardLightingPass::BuildData( const StaticSamplerRange& static_samplers,
											DXGI_FORMAT direct_rtv_format, DXGI_FORMAT ambient_rtv_format, DXGI_FORMAT normal_format, DXGI_FORMAT depth_stencil_format,
											ID3D12Device& device,
											ComPtr<ID3D12PipelineState>& main_pso,
											ComPtr<ID3D12PipelineState>& wireframe_pso,
											ComPtr<ID3D12RootSignature>& rootsig )
{
	const ::InputLayout input_layout = InputLayout();

	const Shaders shaders = LoadAndCompileShaders();
	rootsig = BuildRootSignature( static_samplers, device );

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc;
	ZeroMemory( &pso_desc, sizeof( D3D12_GRAPHICS_PIPELINE_STATE_DESC ) );

	pso_desc.InputLayout = { input_layout.data(), UINT( input_layout.size() ) };
	pso_desc.pRootSignature = rootsig.Get();

	pso_desc.VS =
	{
		reinterpret_cast<BYTE*>( shaders.vs->GetBufferPointer() ),
		shaders.vs->GetBufferSize()
	};

	pso_desc.PS =
	{
		reinterpret_cast<BYTE*>( shaders.ps->GetBufferPointer() ),
		shaders.ps->GetBufferSize()
	};

	pso_desc.GS =
	{
		reinterpret_cast<BYTE*>( shaders.gs->GetBufferPointer() ),
		shaders.gs->GetBufferSize()
	};

	pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC( D3D12_DEFAULT );
	pso_desc.BlendState = CD3DX12_BLEND_DESC( D3D12_DEFAULT );
	pso_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC( D3D12_DEFAULT );
	pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;
	pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	pso_desc.SampleMask = UINT_MAX;
	pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	pso_desc.NumRenderTargets = 3;
	pso_desc.RTVFormats[0] = direct_rtv_format;
	pso_desc.RTVFormats[1] = ambient_rtv_format;
	pso_desc.RTVFormats[2] = normal_format;
	pso_desc.SampleDesc.Count = 1;
	pso_desc.SampleDesc.Quality = 0;

	pso_desc.DSVFormat = depth_stencil_format;

	ThrowIfFailed( device.CreateGraphicsPipelineState( &pso_desc, IID_PPV_ARGS( &main_pso ) ) );

	pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	ThrowIfFailed( device.CreateGraphicsPipelineState( &pso_desc, IID_PPV_ARGS( &wireframe_pso ) ) );
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

