#pragma once

#include "RenderData.h"
#include "RenderPass.h"

#include "Ptr.h"

class SkyboxPass : public RenderPass
{
public:
	SkyboxPass( ID3D12Device& device );

	RenderStateID BuildRenderState( DXGI_FORMAT rtv_format, DXGI_FORMAT dsv_format, ID3D12Device& device );

	struct Context
	{
		D3D12_GPU_VIRTUAL_ADDRESS pass_cb;
		D3D12_GPU_VIRTUAL_ADDRESS skybox_cb;
		D3D12_GPU_DESCRIPTOR_HANDLE skybox_srv;
		D3D12_CPU_DESCRIPTOR_HANDLE frame_rtv;
		D3D12_CPU_DESCRIPTOR_HANDLE frame_dsv;
		float radiance_multiplier;
	};

	void Draw( const Context& context );

private:
	static ComPtr<ID3D12RootSignature> BuildRootSignature( ID3D12Device& device );
	static std::pair<ComPtr<ID3DBlob>, ComPtr<ID3DBlob>> LoadAndCompileShaders();

	virtual void BeginDerived( RenderStateID state ) noexcept override;

	ComPtr<ID3D12RootSignature> m_root_signature = nullptr;
};