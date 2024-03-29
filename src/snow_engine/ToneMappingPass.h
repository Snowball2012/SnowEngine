#pragma once

#include "RenderData.h"
#include "RenderPass.h"

#include "Ptr.h"

class ToneMappingPass : public RenderPass
{
public:
    ToneMappingPass( ID3D12Device& device );

    RenderStateID BuildRenderState( ID3D12Device& device );

    struct ShaderData
    {
        float whitepoint_luminance;
        float enable_reinhard;
    };

    struct Context
    {
        D3D12_GPU_DESCRIPTOR_HANDLE hdr_srv;
        D3D12_GPU_DESCRIPTOR_HANDLE sdr_uav;
        D3D12_GPU_DESCRIPTOR_HANDLE avg_radiance_srv;

        ShaderData gpu_data;
        uint32_t frame_size[2];
    };

    void Draw( const Context& context );

private:
    static ComPtr<ID3D12RootSignature> BuildRootSignature( ID3D12Device& device );
    static ComPtr<ID3DBlob> LoadAndCompileShaders();

    virtual void BeginDerived( RenderStateID state ) noexcept override;

    ComPtr<ID3D12RootSignature> m_root_signature = nullptr;
};