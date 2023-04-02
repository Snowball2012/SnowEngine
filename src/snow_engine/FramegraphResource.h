#pragma once

#include "stdafx.h"

#include "framegraph/Framegraph.h"

#include "RenderData.h"
#include "ToneMappingPass.h"
#include "HBAOPass.h"

#include <tuple>


// resource handles here must be lightweight. Try not to store the data itself here, only copyable handles/pointers with default constructors

struct ShadowProducers
{
    span<ShadowProducer> arr;
};

struct ShadowCascadeProducers
{
    span<ShadowCascadeProducer> arr;
};

struct ShadowMaps : TrackedResource
{
    D3D12_CPU_DESCRIPTOR_HANDLE dsv;
    D3D12_GPU_DESCRIPTOR_HANDLE srv;
};

struct ShadowCascade : TrackedResource
{
    span<const D3D12_CPU_DESCRIPTOR_HANDLE> dsvs;
    D3D12_GPU_DESCRIPTOR_HANDLE srv;
};

struct ForwardPassCB
{
    D3D12_GPU_VIRTUAL_ADDRESS pass_cb;
};

struct HDRBuffer : TrackedResource
{
    DirectX::XMUINT2 size;
    uint32_t nmips;
    D3D12_GPU_DESCRIPTOR_HANDLE srv_all_mips;
    bc::small_vector<D3D12_GPU_DESCRIPTOR_HANDLE, 13> srv; // per mip level, non cumulative
    bc::small_vector<D3D12_CPU_DESCRIPTOR_HANDLE, 13> rtv;
    D3D12_GPU_DESCRIPTOR_HANDLE uav_per_mip_table; // contigious table of mip uavs, starting with mip 0
};

struct HDRBuffer_Final : TrackedResource
{
    D3D12_GPU_DESCRIPTOR_HANDLE srv;
    D3D12_GPU_DESCRIPTOR_HANDLE uav;
};

struct HDRBuffer_Prev : TrackedResource
{
    D3D12_GPU_DESCRIPTOR_HANDLE srv;
};

struct AmbientBuffer : TrackedResource
{
    D3D12_GPU_DESCRIPTOR_HANDLE srv;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv;
};

struct NormalBuffer : TrackedResource
{
    D3D12_GPU_DESCRIPTOR_HANDLE srv;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv;
};

struct Skybox
{
    D3D12_GPU_DESCRIPTOR_HANDLE srv_skybox;
    D3D12_GPU_DESCRIPTOR_HANDLE srv_table; // irradiance, reflection
    D3D12_GPU_VIRTUAL_ADDRESS tf_cbv;
    float radiance_factor;
};

struct SSAOBuffer_Noisy : TrackedResource
{
    D3D12_GPU_DESCRIPTOR_HANDLE srv;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv;
};

struct SSAOTexture_Blurred : TrackedResource
{
    D3D12_GPU_DESCRIPTOR_HANDLE srv;
    D3D12_GPU_DESCRIPTOR_HANDLE uav;
};

struct SSAOTexture_Transposed : TrackedResource
{
    D3D12_GPU_DESCRIPTOR_HANDLE srv;
    D3D12_GPU_DESCRIPTOR_HANDLE uav;
};


struct TonemapNodeSettings
{
    float whitepoint_luminance;
    bool enable_reinhard;
};

struct HBAOSettings
{
    HBAOPass::Settings data;
};

struct SDRBuffer : TrackedResource
{
    D3D12_GPU_DESCRIPTOR_HANDLE uav;
    D3D12_VIEWPORT viewport;
    D3D12_RECT scissor_rect;
};

struct DepthStencilBuffer : TrackedResource
{
    D3D12_CPU_DESCRIPTOR_HANDLE dsv;
    D3D12_GPU_DESCRIPTOR_HANDLE srv;
};

struct DirectShadowsMask : TrackedResource
{
    D3D12_GPU_DESCRIPTOR_HANDLE uav;
    D3D12_GPU_DESCRIPTOR_HANDLE srv;
};

struct SceneRaytracingTLAS : TrackedResource
{
};

struct MotionVectors : TrackedResource
{
    D3D12_GPU_DESCRIPTOR_HANDLE srv;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv;
};

struct ScreenConstants
{
    D3D12_VIEWPORT viewport;
    D3D12_RECT scissor_rect;
};

struct MainRenderitems
{
    span<const span<const RenderBatch>> items;
};

struct SkyboxData
{
    float radiance_factor = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE cubemap_srv = {};
    DirectX::XMFLOAT4X4 obj2world_mat = {};
};

struct GlobalAtomicBuffer : TrackedResource
{
    D3D12_GPU_DESCRIPTOR_HANDLE uav;
    D3D12_CPU_DESCRIPTOR_HANDLE uav_cpu_handle; // must be in a non-shader visible heap!
};

struct SinglePassDownsamplerShaderCB
{
    D3D12_GPU_VIRTUAL_ADDRESS gpu_ptr;
    span<uint8_t> mapped_region;
};

struct HDRLightingMain {};