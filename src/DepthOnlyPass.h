#pragma once

#include "Ptr.h"

#include "RenderData.h"

#include "RenderPass.h"

#include "FramegraphResource.h"

class DepthOnlyPass : public RenderPass
{
public:
    DepthOnlyPass( ID3D12Device& device );

    RenderStateID BuildRenderState( DXGI_FORMAT dsv_format,
                                    int bias, bool reversed_z,
                                    bool back_culling, ID3D12Device& device );

    struct Context
    {
        span<const span<const RenderBatch>> renderitems;
        D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil_view;
        D3D12_GPU_VIRTUAL_ADDRESS pass_cbv;
    };

    void Draw( const Context& context );

private:

    using Shaders = std::pair<ComPtr<ID3DBlob>, ComPtr<ID3DBlob>>;

    static ComPtr<ID3D12RootSignature> BuildRootSignature( ID3D12Device& device );
    static Shaders LoadAndCompileShaders();

    virtual void BeginDerived( RenderStateID state ) noexcept override;

    ComPtr<ID3D12RootSignature> m_root_signature = nullptr;
};