#pragma once

#include "RenderData.h"

#include "RenderPass.h"

class ForwardLightingPass : public RenderPass
{
public:
	ForwardLightingPass( ID3D12Device& device );

	struct States
	{
		RenderStateID triangle_fill;
		RenderStateID wireframe;
	};

	States CompileStates( DXGI_FORMAT direct_format,
						  DXGI_FORMAT ambient_format,
						  DXGI_FORMAT normal_format,
						  DXGI_FORMAT depth_stencil_format,
						  ID3D12Device& device );

	struct Context
	{
		span<RenderItem> renderitems;
		D3D12_CPU_DESCRIPTOR_HANDLE back_buffer_rtv;
		D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil_view;
		D3D12_CPU_DESCRIPTOR_HANDLE ambient_rtv;
		D3D12_CPU_DESCRIPTOR_HANDLE normals_rtv;
		D3D12_GPU_DESCRIPTOR_HANDLE shadow_map_srv;
		D3D12_GPU_DESCRIPTOR_HANDLE shadow_cascade_srv;
		D3D12_GPU_VIRTUAL_ADDRESS pass_cb;
	};

	// all descriptor heaps must be set prematurely
	void Draw( const Context& context ) noexcept;

	static inline InputLayout InputLayout() noexcept;

private:
	virtual void BeginDerived( RenderStateID state ) noexcept override;

	struct Shaders
	{
		ComPtr<ID3DBlob> vs;
		ComPtr<ID3DBlob> gs;
		ComPtr<ID3DBlob> ps;
	};
	static Shaders LoadAndCompileShaders();

	template<class StaticSamplerRange>
	static ComPtr<ID3D12RootSignature> BuildRootSignature( const StaticSamplerRange& static_samplers, ID3D12Device& device );

	ComPtr<ID3D12RootSignature> m_root_signature = nullptr;
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
		4 - shadow cascade
		5 - cbv per pass

		Shader register bindings
		b0 - cbv per obj
		b1 - cbv per material
		b2 - cbv per pass

		t0 - albedo
		t1 - normal
		t2 - specular
		t3 - shadow
		t4 - shadow cascade

		To optimize state changes keep material textures together when possible. Descriptor duplication?
	*/
	constexpr int nparams = 6;

	CD3DX12_ROOT_PARAMETER1 slot_root_parameter[nparams];

	slot_root_parameter[0].InitAsConstantBufferView( 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_VERTEX );
	slot_root_parameter[1].InitAsConstantBufferView( 1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_PIXEL );

	CD3DX12_DESCRIPTOR_RANGE1 desc_table[3];
	desc_table[0].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC ); // albedo, normal, specular
	desc_table[1].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3 ); // shadow
	desc_table[2].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4 ); // shadow cascade

	slot_root_parameter[2].InitAsDescriptorTable( 1, &desc_table[0], D3D12_SHADER_VISIBILITY_PIXEL );
	slot_root_parameter[3].InitAsDescriptorTable( 1, &desc_table[1], D3D12_SHADER_VISIBILITY_PIXEL );
	slot_root_parameter[4].InitAsDescriptorTable( 1, &desc_table[2], D3D12_SHADER_VISIBILITY_PIXEL );

	slot_root_parameter[5].InitAsConstantBufferView( 2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC );

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_sig_desc( nparams, slot_root_parameter,
	                                                     UINT( static_samplers.size() ), static_samplers.data(),
	                                                     D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT );

	ComPtr<ID3DBlob> serialized_root_sig = nullptr;
	ComPtr<ID3DBlob> error_blob = nullptr;
	HRESULT hr = D3DX12SerializeVersionedRootSignature( &root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1_1,
													   serialized_root_sig.GetAddressOf(),
													   error_blob.GetAddressOf() );

	if ( error_blob )
		OutputDebugStringA( (char*)error_blob->GetBufferPointer() );

	ThrowIfFailed( hr );

	ComPtr<ID3D12RootSignature> rootsig;

	ThrowIfFailed( device.CreateRootSignature(
		0,
		serialized_root_sig->GetBufferPointer(),
		serialized_root_sig->GetBufferSize(),
		IID_PPV_ARGS( &rootsig ) ) );

	return rootsig;
}


inline InputLayout ForwardLightingPass::InputLayout() noexcept
{
	return
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}
