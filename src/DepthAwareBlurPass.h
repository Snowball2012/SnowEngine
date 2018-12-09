#pragma once

#include "Ptr.h"

// Separable Gaussian blur
// Strictly speaking this kind of bilateral filter is not separable, but the results are passable anyway
// As of now the pass is not actually depth-aware =)

class DepthAwareBlurPass
{
public:

	DepthAwareBlurPass( ID3D12PipelineState* pso, ID3D12RootSignature* rootsig );

	struct Context
	{
		D3D12_GPU_DESCRIPTOR_HANDLE input_srv;
		D3D12_GPU_DESCRIPTOR_HANDLE depth_srv;
		D3D12_GPU_DESCRIPTOR_HANDLE blurred_uav;
		uint32_t uav_width;
		uint32_t uav_height;
	};

	void Draw( const Context& context, ID3D12GraphicsCommandList& cmd_list );

	static ComPtr<ID3D12RootSignature> BuildRootSignature( ID3D12Device& device );
	static void BuildData( ID3D12Device& device, ComPtr<ID3D12PipelineState>& pso, ComPtr<ID3D12RootSignature>& rootsig );
	static ComPtr<ID3DBlob> LoadAndCompileShader();

private:

	static constexpr uint32_t GroupSizeX = 64;
	static constexpr uint32_t GroupSizeY = 4;

	ID3D12PipelineState* m_pso = nullptr;
	ID3D12RootSignature* m_root_signature = nullptr;
};