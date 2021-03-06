#pragma once

#include "RenderData.h"
#include "RenderPass.h"

#include "Ptr.h"

// Performs a single-pass texture downscale (max 13 mips)
class SingleDownsamplerPass : public RenderPass
{
public:
    SingleDownsamplerPass( ID3D12Device& device );

    RenderStateID BuildRenderState( ID3D12Device& device );

    static constexpr uint32_t MaxMips = 13;

    struct Context
    {
        bc::static_vector<DirectX::XMUINT2, MaxMips> mip_size;
        D3D12_GPU_DESCRIPTOR_HANDLE mip_uav_table;
        D3D12_GPU_DESCRIPTOR_HANDLE global_atomic_counter_uav; // must already be zero-initialized
        D3D12_GPU_VIRTUAL_ADDRESS shader_cb_gpu;
        span<uint8_t> shader_cb_mapped;
    };

    struct alignas(16) ShaderCB
    {
        uint32_t nmips;
        uint32_t ngroups;
        uint32_t _pad[2];
        uint32_t mip_size[SingleDownsamplerPass::MaxMips * 4]; // last 2 uints are empty in each entry
    };

    void Draw( const Context& context );

private:
    static ComPtr<ID3D12RootSignature> BuildRootSignature( ID3D12Device& device );
    static ComPtr<ID3DBlob> LoadAndCompileShader();

    virtual void BeginDerived( RenderStateID state ) noexcept override;

    ComPtr<ID3D12RootSignature> m_root_signature = nullptr;
};