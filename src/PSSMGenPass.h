#pragma once

#include "Ptr.h"

#include "RenderData.h"

class PSSMGenPass
{
public:
	PSSMGenPass( ID3D12PipelineState* pso, ID3D12RootSignature* rootsig ) noexcept;

	struct Context
	{
		span<const RenderItem> renderitems;
		D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil_view;
		D3D12_GPU_VIRTUAL_ADDRESS pass_cbv;
		uint32_t light_idx;
	};

	void Draw( const Context& context, ID3D12GraphicsCommandList& cmd_list ) noexcept;

	static ComPtr<ID3D12RootSignature> BuildRootSignature( ID3D12Device& device );

	static void BuildData( DXGI_FORMAT dsv_format, int bias, bool back_culling, ID3D12Device& device, ComPtr<ID3D12PipelineState>& pso, ComPtr<ID3D12RootSignature>& rootsig );

	struct Shaders
	{
		ComPtr<ID3DBlob> vs;
		ComPtr<ID3DBlob> gs;
		ComPtr<ID3DBlob> ps;
	};

	static Shaders LoadAndCompileShaders();

	// uses input layout from ForwardLightingPass
private:
	ID3D12PipelineState* m_pso = nullptr;
	ID3D12RootSignature* m_root_signature = nullptr;
};