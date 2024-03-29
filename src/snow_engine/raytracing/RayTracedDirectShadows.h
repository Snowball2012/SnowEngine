﻿#pragma once

#include <snow_engine/FramegraphResource.h>

class GPUDevice;

class GenerateRaytracedShadowmaskPass
{
public:
    struct Context
    {
        uint32_t uav_width = 0;
        uint32_t uav_height = 0;

        D3D12_GPU_DESCRIPTOR_HANDLE shadowmask_uav;
        D3D12_GPU_DESCRIPTOR_HANDLE depth_srv;
        D3D12_GPU_VIRTUAL_ADDRESS tlas;

        DirectX::XMFLOAT3 light_dir;

        D3D12_GPU_VIRTUAL_ADDRESS pass_cb;
    };

private:
    ComPtr<ID3D12Device5> m_rt_device = nullptr;
    ComPtr<ID3D12RootSignature> m_global_root_signature = nullptr;
    ComPtr<ID3DBlob> m_global_root_signature_serialized = nullptr;
    ComPtr<ID3D12Resource> m_shader_table = nullptr;
    ComPtr<ID3D12StateObject> m_rtpso = nullptr;
    ComPtr<ID3D12StateObjectProperties> m_rtpso_info = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS m_raygen_shader_record = -1;
    D3D12_GPU_VIRTUAL_ADDRESS m_miss_shader_record = -1;
    D3D12_GPU_VIRTUAL_ADDRESS m_hitgroup_shader_record = -1;
    size_t m_shader_record_size = 0;
    
public:

    GenerateRaytracedShadowmaskPass(GPUDevice& device);

    void Draw(const Context& context, IGraphicsCommandList& cmd_list);

private:

    void GenerateShaderTable(ID3D12Device& device);
    
    static ComPtr<ID3D12RootSignature> BuildRootSignature(ID3D12Device& device, ComPtr<ID3DBlob>& serialized_rs);

    void BuildRaytracingPSO(ID3D12Device5& device);
};

template<class Framegraph>
class GenerateRaytracedShadowmaskNode : public BaseRenderNode<Framegraph>
{
public:
    using OpenRes = std::tuple<>;
    using WriteRes = std::tuple<
        ResourceInState<DirectShadowsMask, D3D12_RESOURCE_STATE_UNORDERED_ACCESS>
    >;
    using ReadRes = std::tuple<
        ResourceInState<DepthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_READ>,
        ResourceInState<SceneRaytracingTLAS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE>,
        ForwardPassCB
    >;
    using CloseRes = std::tuple<>;
    
private:    
    GenerateRaytracedShadowmaskPass m_pass;
    
public:

    GenerateRaytracedShadowmaskNode(GPUDevice& device)
        : m_pass(device)
    {
    }
    
    virtual void Run(Framegraph& framegraph, IGraphicsCommandList& cmd_list) override;
};

template <class Framegraph>
void GenerateRaytracedShadowmaskNode<Framegraph>::Run(Framegraph& framegraph, IGraphicsCommandList& cmd_list)
{
    OPTICK_EVENT();

    auto& depth_buffer = framegraph.GetRes<DepthStencilBuffer>();
    auto& shadowmask = framegraph.GetRes<DirectShadowsMask>();
    auto& pass_cb = framegraph.GetRes<ForwardPassCB>();
    auto& scene_tlas = framegraph.GetRes<SceneRaytracingTLAS>();

    const auto& shadowmask_desc = shadowmask->res->GetDesc();

    if (!depth_buffer || !shadowmask || !scene_tlas)
        throw SnowEngineException("missing resource");

    if (!scene_tlas->res)
        return;
    
    PIXBeginEvent(&cmd_list, PIX_COLOR( 200, 210, 230 ), "Raytraced Direct Shadows");

    GenerateRaytracedShadowmaskPass::Context ctx;
    ctx.depth_srv = depth_buffer->srv;
    ctx.shadowmask_uav = shadowmask->uav;
    ctx.tlas = scene_tlas->res->GetGPUVirtualAddress();

    ctx.uav_width = shadowmask_desc.Width;
    ctx.uav_height = shadowmask_desc.Height;
    ctx.pass_cb = pass_cb->pass_cb;

    m_pass.Draw(ctx, cmd_list);
    
    PIXEndEvent(&cmd_list);
}
