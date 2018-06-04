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
		RenderSceneContext* scene;
		D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil_view;
		ID3D12Resource* pass_cb;
		int pass_cb_idx;
		ID3D12Resource* object_cb;
	};

	void Draw( const Context& context, ID3D12GraphicsCommandList& cmd_list );

	static ComPtr<ID3D12RootSignature> BuildRootSignature( ID3D12Device& device );

	static void BuildData( DXGI_FORMAT dsv_format, int bias, bool back_culling, ID3D12Device& device, ComPtr<ID3D12PipelineState>& pso, ComPtr<ID3D12RootSignature>& rootsig );

	static ComPtr<ID3DBlob> LoadAndCompileVertexShader();

	// uses input layout from ForwardLightingPass

private:
	ID3D12PipelineState* m_pso = nullptr;
	ID3D12RootSignature* m_root_signature = nullptr;
};