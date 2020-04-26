#pragma once

#include "RenderData.h"
#include "RenderPass.h"

#include "Ptr.h"

class ToneMappingPass : public RenderPass
{
public:
    ToneMappingPass( ID3D12Device& device );

    RenderStateID BuildRenderState( DXGI_FORMAT rtv_format, ID3D12Device& device );

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

    void Draw( const Context& context );

private:
    static ComPtr<ID3D12RootSignature> BuildRootSignature( ID3D12Device& device );
    static std::pair<ComPtr<ID3DBlob>, ComPtr<ID3DBlob>> LoadAndCompileShaders();

    virtual void BeginDerived( RenderStateID state ) noexcept override;

    ComPtr<ID3D12RootSignature> m_root_signature = nullptr;
};