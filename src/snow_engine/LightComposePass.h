#pragma once

#include "RenderData.h"
#include "RenderPass.h"

#include "Ptr.h"


// Takes an HDR target as input along with ambient lighting and ssao and combines them to one buffer
// Uses point sampler so it's better to use textures of the same resolution
class LightComposePass : public RenderPass
{
public:
    LightComposePass( ID3D12Device& device );

    RenderStateID BuildRenderState( DXGI_FORMAT rtv_format, ID3D12Device& device );

    struct Context
    {
        D3D12_GPU_DESCRIPTOR_HANDLE ambient_srv;
        D3D12_GPU_DESCRIPTOR_HANDLE ssao_srv;
        D3D12_CPU_DESCRIPTOR_HANDLE frame_rtv;
    };

    void Draw( const Context& context );

private:
    static ComPtr<ID3D12RootSignature> BuildRootSignature( ID3D12Device& device );
    static std::pair<ComPtr<ID3DBlob>, ComPtr<ID3DBlob>> LoadAndCompileShaders();

    virtual void BeginDerived( RenderStateID state ) noexcept override;

    ComPtr<ID3D12RootSignature> m_root_signature = nullptr;
};