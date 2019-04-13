#pragma once

#include "Ptr.h"

#include "RenderData.h"

#include "RenderPass.h"

class PSSMGenPass : public RenderPass
{
public:
    PSSMGenPass( ID3D12Device& device );

    RenderStateID BuildRenderState( DXGI_FORMAT dsv_format, int bias, bool back_culling, ID3D12Device& device );

    struct Context
    {
        span<const RenderItem> renderitems;
        D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil_view;
        D3D12_GPU_VIRTUAL_ADDRESS pass_cbv;
        uint32_t light_idx;
    };

    void Draw( const Context& context ) noexcept;

    // uses input layout from ForwardLightingPass
private:
    struct Shaders
    {
        ComPtr<ID3DBlob> vs;
        ComPtr<ID3DBlob> gs;
        ComPtr<ID3DBlob> ps;
    };

    static ComPtr<ID3D12RootSignature> BuildRootSignature( ID3D12Device& device );
    static Shaders LoadAndCompileShaders();

    virtual void BeginDerived( RenderStateID state ) noexcept override;

    ComPtr<ID3D12RootSignature> m_root_signature = nullptr;
};