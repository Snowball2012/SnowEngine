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
    D3D12_CPU_DESCRIPTOR_HANDLE dsv;
    D3D12_GPU_DESCRIPTOR_HANDLE srv;
};

struct ForwardPassCB
{
    D3D12_GPU_VIRTUAL_ADDRESS pass_cb;
};

struct HDRBuffer : TrackedResource
{
    D3D12_GPU_DESCRIPTOR_HANDLE srv;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv;
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
    ToneMappingPass::ShaderData data;
};

struct HBAOSettings
{
    HBAOPass::Settings data;
};

struct SDRBuffer : TrackedResource
{
    D3D12_CPU_DESCRIPTOR_HANDLE rtv;
    D3D12_VIEWPORT viewport;
    D3D12_RECT scissor_rect;
};

struct DepthStencilBuffer : TrackedResource
{
    D3D12_CPU_DESCRIPTOR_HANDLE dsv;
    D3D12_GPU_DESCRIPTOR_HANDLE srv;
};

struct ScreenConstants
{
    D3D12_VIEWPORT viewport;
    D3D12_RECT scissor_rect;
};

struct MainRenderitems
{
    span<RenderItem> items;
};


// UNDER CONSTRUCTION
enum class FramegraphTechnique
{
    ShadowGenPass,
    ForwardZPass
};

struct IRenderMaterial
{
    // first - pso id, second - rootsig id
    virtual std::pair<uint64_t, uint64_t> GetPipelineStateID( FramegraphTechnique technique ) const = 0;

    // todo: replace D3D12_ROOT_PARAMETER with actual shader parameter class
    virtual bool BindDataToPipeline( const span<D3D12_ROOT_PARAMETER>& data ) const = 0;
};

struct IRenderGeom
{
};

struct RenderBatch
{
    const IRenderMaterial* material = nullptr;
    const IRenderGeom* geom = nullptr;

    uint32_t index_count = 0;
    uint32_t index_offset = 0;
    uint32_t vertex_offset = 0;
    uint32_t instance_count = 0;

    D3D12_GPU_VIRTUAL_ADDRESS per_object_cb = 0; // contents of this buffer may vary depending on the passes this batch participates in
    D3D12_GPU_VIRTUAL_ADDRESS custom_per_object_cb = 0; // some materials may require various per object data

    D3D12_BOX bounding_box = {};
};

struct RenderItem_New
{
    const IRenderMaterial* material = nullptr;
    const IRenderGeom* geom = nullptr;

    uint32_t index_count = 0;
    uint32_t index_offset = 0;
    uint32_t vertex_offset = 0;
    
    D3D12_GPU_VIRTUAL_ADDRESS custom_per_object_cb = 0; // some materials may require various per object data
    
    D3D12_BOX bounding_box = {};
    DirectX::XMFLOAT4X4 local2world = {};
};

struct SkyboxData
{
    float radiance_factor = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE cubemap_srv = {};
    DirectX::XMFLOAT4X4 obj2world_mat = {};
};