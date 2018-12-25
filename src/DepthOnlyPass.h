#pragma once

#include "RenderData.h"

#include <wrl.h>
template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

class DepthOnlyPass
{
public:
	DepthOnlyPass( ID3D12PipelineState* pso, ID3D12RootSignature* rootsig );

	struct Context
	{
		const std::vector<RenderItem>* renderitems;
		D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil_view;
		D3D12_GPU_VIRTUAL_ADDRESS pass_cbv;
	};

	void Draw( const Context& context, ID3D12GraphicsCommandList& cmd_list );

	static ComPtr<ID3D12RootSignature> BuildRootSignature( ID3D12Device& device );

	static void BuildData( DXGI_FORMAT dsv_format, int bias, bool reversed_z, bool back_culling, ID3D12Device& device, ComPtr<ID3D12PipelineState>& pso, ComPtr<ID3D12RootSignature>& rootsig );

	using Shaders = std::pair<ComPtr<ID3DBlob>, ComPtr<ID3DBlob>>;

	static Shaders LoadAndCompileShaders();

	// uses input layout from ForwardLightingPass

private:
	ID3D12PipelineState* m_pso = nullptr;
	ID3D12RootSignature* m_root_signature = nullptr;
};