#pragma once

#include "RenderData.h"
#include "RenderPass.h"

#include "Ptr.h"

// Performs a texture downscale. The source texture must be 2x size of the destination
class DownscalePass : public RenderPass
{
public:
    DownscalePass( ID3D12Device& device );

    RenderStateID BuildRenderState( DXGI_FORMAT rtv_format, ID3D12Device& device );

    struct Context
    {
        D3D12_GPU_DESCRIPTOR_HANDLE src_srv;
        D3D12_CPU_DESCRIPTOR_HANDLE dst_rtv;
    };

    void Draw( const Context& context );

private:
    static ComPtr<ID3D12RootSignature> BuildRootSignature( ID3D12Device& device );
    static std::pair<ComPtr<ID3DBlob>, ComPtr<ID3DBlob>> LoadAndCompileShaders();

    virtual void BeginDerived( RenderStateID state ) noexcept override;

    ComPtr<ID3D12RootSignature> m_root_signature = nullptr;
};