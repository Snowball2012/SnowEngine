#pragma once

#include "Ptr.h"

#include "RenderPass.h"

// Separable Gaussian blur
// Strictly speaking this kind of bilateral filter is not separable, but the results are passable anyway

class DepthAwareBlurPass : public RenderPass
{
public:
    DepthAwareBlurPass( ID3D12Device& device );

    RenderStateID BuildRenderState( ID3D12Device& device );

    struct Context
    {
        D3D12_GPU_DESCRIPTOR_HANDLE input_srv;
        D3D12_GPU_DESCRIPTOR_HANDLE depth_srv;
        D3D12_GPU_DESCRIPTOR_HANDLE blurred_uav;
        D3D12_GPU_VIRTUAL_ADDRESS pass_cb;
        uint32_t uav_width;
        uint32_t uav_height;
        bool transpose_flag;
    };

    void Draw( const Context& context );

private:
    static constexpr uint32_t GroupSizeX = 64;
    static constexpr uint32_t GroupSizeY = 4;

    virtual void BeginDerived( RenderStateID state ) noexcept override;

    static ComPtr<ID3D12RootSignature> BuildRootSignature( ID3D12Device& device );
    static ComPtr<ID3DBlob> LoadAndCompileShader();

    ComPtr<ID3D12RootSignature> m_root_signature = nullptr;
};