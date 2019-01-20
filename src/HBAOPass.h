#pragma once

#include "RenderData.h"

#include "RenderPass.h"

#include "Ptr.h"

class HBAOPass : public RenderPass
{
public:
	HBAOPass( ID3D12Device& device );

	RenderStateID BuildRenderState( DXGI_FORMAT rtv_format, ID3D12Device& device );

	struct Settings
	{
		DirectX::XMFLOAT2 render_target_size;
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

	void Draw( const Context& context );

private:

	static ComPtr<ID3D12RootSignature> BuildRootSignature( ID3D12Device& device );
	static std::pair<ComPtr<ID3DBlob>, ComPtr<ID3DBlob>> LoadAndCompileShaders();

	virtual void BeginDerived( RenderStateID state ) noexcept override;

	ComPtr<ID3D12RootSignature> m_root_signature = nullptr;
};