#pragma once

#include "RenderUtils.h"

#include <DirectXCollision.h>

#include "DescriptorHeap.h"

#include "DescriptorTableBakery.h"

#include <utils/MathUtils.h>

#include "DynamicTexture.h"

enum class FramegraphTechnique
{
    ShadowGenPass,
    ForwardZPass,
    DepthPass,
};

struct IRenderMaterial
{
    // first - pso id, second - rootsig id
    virtual std::pair<uint64_t, uint64_t> GetPipelineStateID( FramegraphTechnique technique ) const = 0;

    // todo: replace command list with something less spaghetti(some parameter class)
    virtual bool BindDataToPipeline( FramegraphTechnique technique, uint64_t item_id, IGraphicsCommandList& cmd_list ) const = 0;
};

struct RenderBatch
{
    const IRenderMaterial* material = nullptr;

    D3D12_VERTEX_BUFFER_VIEW vbv = {};
    D3D12_INDEX_BUFFER_VIEW ibv = {};

    uint32_t index_count = 0;
    uint32_t index_offset = 0;
    uint32_t vertex_offset = 0;
    uint32_t instance_count = 0;

    D3D12_GPU_VIRTUAL_ADDRESS per_object_cb = 0; // contents of this buffer may vary depending on the passes this batch participates in

    uint64_t item_id = 0; // some id for a material to find object-specific info
};

struct RenderItem
{
    const IRenderMaterial* material = nullptr;

    D3D12_VERTEX_BUFFER_VIEW vbv = {};
    D3D12_INDEX_BUFFER_VIEW ibv = {};

    uint32_t index_count = 0;
    uint32_t index_offset = 0;
    uint32_t vertex_offset = 0;
    
    uint64_t item_id = 0; // some id for a material to find object-specific info

    DirectX::XMFLOAT4X4 local2world = {};
};

struct ShadowMapGenData
{
    D3D12_GPU_VIRTUAL_ADDRESS pass_cb;
    D3D12_VIEWPORT viewport;
};

struct ShadowProducer
{
    ShadowMapGenData map_data;
    std::vector<RenderBatch> casters;
};

struct ShadowCascadeProducer
{
    D3D12_VIEWPORT viewport;
    uint32_t light_idx_in_cb;
    bc::small_vector<std::vector<RenderBatch>, 4> casters;
};

struct GPUObjectConstants
{
    DirectX::XMFLOAT4X4 model;
    DirectX::XMFLOAT4X4 model_inv_transpose;
};

struct MaterialConstants
{
    DirectX::XMFLOAT4X4 mat_transform;
	DirectX::XMFLOAT4 albedo_color;
    DirectX::XMFLOAT3 diffuse_fresnel;
};

struct LightConstants
{
    DirectX::XMFLOAT4X4 shadow_map_matrix;
    DirectX::XMFLOAT3 strength;
    float falloff_start; // point and spotlight
    DirectX::XMFLOAT3 origin; // point and spotlight
    float falloff_end; // point and spotlight
    DirectX::XMFLOAT3 dir; // spotlight, direction TO the light source
    float spot_power; // spotlight only
};

constexpr uint32_t MAX_CASCADE_SIZE = 4;


struct ParallelLightConstants
{
    DirectX::XMFLOAT4X4 shadow_map_matrix[MAX_CASCADE_SIZE];

    DirectX::XMFLOAT3 strength; // spectral irradiance, in watt/sq.meter (visible spectrum only)
    int32_t csm_num_split_positions;     // must be <= MaxCascadeSize

    DirectX::XMFLOAT3 dir;      // to the light source
    float half_angle_sin_2;
};

constexpr uint32_t MAX_LIGHTS = 15;
constexpr uint32_t MAX_CSM_LIGHTS = 1;

struct alignas( 16 ) GPUPassConstants
{
    DirectX::XMFLOAT4X4 view_mat = Identity4x4;
    DirectX::XMFLOAT4X4 view_inv_mat = Identity4x4;
    DirectX::XMFLOAT4X4 proj_mat = Identity4x4;
    DirectX::XMFLOAT4X4 proj_inv_mat = Identity4x4;
    DirectX::XMFLOAT4X4 view_proj_mat = Identity4x4;
    DirectX::XMFLOAT4X4 view_proj_mat_unjittered = Identity4x4;
    DirectX::XMFLOAT4X4 view_proj_mat_prev = Identity4x4;
    
    DirectX::XMFLOAT4X4 view_proj_inv_mat = Identity4x4;

    DirectX::XMFLOAT3 eye_pos_w = { 0.0f, 0.0f, 0.0f };
    float _padding1 = 0.0f;

    DirectX::XMFLOAT2 render_target_size = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 render_target_size_inv = { 0.0f, 0.0f };

    float near_z = 0.0f;
    float far_z = 0.0f;
    float fov_y = 0.0f;
    float aspect_ratio = 0.0f;

    float total_time = 0.0f;
    float delta_time = 0.0f;
    DirectX::XMFLOAT2 _padding2;

    LightConstants lights[MAX_LIGHTS];

    ParallelLightConstants parallel_lights[MAX_CSM_LIGHTS];
    float csm_split_positions[( MAX_CASCADE_SIZE - 1 ) * 4];

    int32_t n_parallel_lights = 0;
    int32_t n_point_lights = 0;
    int32_t n_spotlight_lights = 0;
    int32_t _padding3 = 0;
};

class Light;

struct LightInCB
{
    uint32_t light_idx_in_cb;
    const Light* light;
};

using InputLayout = std::vector<D3D12_INPUT_ELEMENT_DESC>;
