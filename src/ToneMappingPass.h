#pragma once

#include "RenderData.h"

#include <wrl.h>
template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

class ToneMappingPass
{
public:
	ToneMappingPass( ID3D12PipelineState* pso, ID3D12RootSignature* rootsig );

	struct ShaderData
	{
		float upper_luminance_bound = 2.e4f;
		float lower_luminance_bound = 1.e-2f;
		bool blend_luminance = false;
	};

	struct Context
	{
		D3D12_GPU_DESCRIPTOR_HANDLE frame_srv;
		D3D12_CPU_DESCRIPTOR_HANDLE frame_rtv;
		ShaderData gpu_data;
	};

	void Draw( const Context& context, ID3D12GraphicsCommandList& cmd_list );

	static ComPtr<ID3D12RootSignature> BuildRootSignature( ID3D12Device& device );

	static void BuildData( DXGI_FORMAT rtv_format, ID3D12Device& device, ComPtr<ID3D12PipelineState>& pso, ComPtr<ID3D12RootSignature>& rootsig );

	static std::pair<ComPtr<ID3DBlob>, ComPtr<ID3DBlob>> LoadAndCompileShaders();

private:
	ID3D12PipelineState* m_pso = nullptr;
	ID3D12RootSignature* m_root_signature = nullptr;
};