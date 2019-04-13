#pragma once

#include "RenderUtils.h"

#include <DirectXCollision.h>

#include "DescriptorHeap.h"

#include "DescriptorTableBakery.h"

#include "ResizableTexture.h"

#include "Scene.h"

// resides only in GPU mem
class DynamicTexture : public ResizableTexture
{
public:
    DynamicTexture( ComPtr<ID3D12Resource>&& texture, ID3D12Device* device,
                    D3D12_RESOURCE_STATES initial_state, const D3D12_CLEAR_VALUE* opt_clear_value ) noexcept
        : ResizableTexture( std::move( texture ), device, initial_state, opt_clear_value )
    {}

    DescriptorTableBakery::TableID SRV() const { return m_srv; }
    DescriptorTableBakery::TableID UAV() const { return m_uav; }
    const Descriptor* RTV() const { return m_rtv.get(); }
    const Descriptor* DSV() const { return m_dsv.get(); }

    DescriptorTableBakery::TableID& SRV() { return m_srv; }
    DescriptorTableBakery::TableID& UAV() { return m_uav; }
    std::unique_ptr<Descriptor>& RTV() { return m_rtv; }
    std::unique_ptr<Descriptor>& DSV() { return m_dsv; }

private:
    DescriptorTableBakery::TableID m_srv = DescriptorTableBakery::TableID::nullid;
    DescriptorTableBakery::TableID m_uav = DescriptorTableBakery::TableID::nullid;
    std::unique_ptr<Descriptor> m_rtv = nullptr;
    std::unique_ptr<Descriptor> m_dsv = nullptr;
};

struct RenderItem
{
    D3D12_VERTEX_BUFFER_VIEW vbv;
    D3D12_INDEX_BUFFER_VIEW ibv;
    uint32_t index_count = 0;
    uint32_t index_offset = 0;
    uint32_t vertex_offset = 0;
    
    D3D12_GPU_VIRTUAL_ADDRESS mat_cb;
    D3D12_GPU_DESCRIPTOR_HANDLE mat_table;

    D3D12_GPU_VIRTUAL_ADDRESS tf_addr;
};

struct ShadowMapGenData
{
    D3D12_GPU_VIRTUAL_ADDRESS pass_cb;
    D3D12_VIEWPORT viewport;
};

struct ShadowProducer
{
    ShadowMapGenData map_data;
    std::vector<RenderItem> casters;
};

struct ShadowCascadeProducer
{
    D3D12_VIEWPORT viewport;
    uint32_t light_idx_in_cb;
    std::vector<RenderItem> casters;
};

struct ObjectConstants
{
    DirectX::XMFLOAT4X4 model = Identity4x4;
    DirectX::XMFLOAT4X4 model_inv_transpose = Identity4x4;
};

struct MaterialConstants
{
    DirectX::XMFLOAT4X4 mat_transform;
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
    float _padding;
};

constexpr uint32_t MAX_LIGHTS = 15;
constexpr uint32_t MAX_CSM_LIGHTS = 1;

struct alignas( 16 ) PassConstants
{
    DirectX::XMFLOAT4X4 view_mat = Identity4x4;
    DirectX::XMFLOAT4X4 view_inv_mat = Identity4x4;
    DirectX::XMFLOAT4X4 proj_mat = Identity4x4;
    DirectX::XMFLOAT4X4 proj_inv_mat = Identity4x4;
    DirectX::XMFLOAT4X4 view_proj_mat = Identity4x4;
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

struct LightInCB
{
    uint32_t light_idx_in_cb;
    const SceneLight* light;
};

// scene representation for renderer
struct RenderSceneContext
{
    std::unordered_map<std::string, StaticSubmeshID> static_geometry;

    // lighting, materials and textures
    std::unordered_map<std::string, MaterialID> materials;
    std::unordered_map<std::string, TextureID> textures;
};

using InputLayout = std::vector<D3D12_INPUT_ELEMENT_DESC>;
