#pragma once

#include "RenderData.h"

#include <wrl.h>
template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

class HBAOPass
{
public:
	HBAOPass( ID3D12PipelineState* pso, ID3D12RootSignature* rootsig );

	struct Settings
	{
		float max_r = 0.20f;
		float angle_bias = DirectX::XM_PIDIV2 / 10.0f;
		int nsamples_per_direction = 4;
	};

	struct Context
	{
		Settings settings;
		D3D12_GPU_DESCRIPTOR_HANDLE depth_srv;
		D3D12_GPU_DESCRIPTOR_HANDLE normals_srv;
		D3D12_GPU_VIRTUAL_ADDRESS pass_cb;
		D3D12_CPU_DESCRIPTOR_HANDLE ssao_rtv;
	};

	void Draw( const Context& context, ID3D12GraphicsCommandList& cmd_list );

	static ComPtr<ID3D12RootSignature> BuildRootSignature( ID3D12Device& device );

	static void BuildData( DXGI_FORMAT rtv_format, ID3D12Device& device, ComPtr<ID3D12PipelineState>& pso, ComPtr<ID3D12RootSignature>& rootsig );

	static std::pair<ComPtr<ID3DBlob>, ComPtr<ID3DBlob>> LoadAndCompileShaders();

private:
	ID3D12PipelineState* m_pso = nullptr;
	ID3D12RootSignature* m_root_signature = nullptr;
};