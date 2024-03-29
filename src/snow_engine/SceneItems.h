#pragma once

#include <d3d12.h>
#include <DirectXCollision.h>
#include <DirectXMath.h>
#include <boost/container/small_vector.hpp>

#include "RenderData.h"

#include "resources/Mesh.h"

#include "utils/packed_freelist.h"

class Scene;

class RefCounter
{
public:
    RefCounter() noexcept = default;
    RefCounter( const RefCounter& ) = delete;
    RefCounter& operator=( const RefCounter& ) = delete;

    RefCounter( RefCounter&& ) noexcept = default;
    RefCounter& operator=( RefCounter&& rhs ) noexcept = default;

    // Adds one ref
    void AddRef() noexcept { m_refs++; }
    // Releases one ref, returns true if there are references left
    bool ReleaseRef() noexcept { if ( m_refs ) m_refs--; return m_refs; }

    uint32_t GetRefCount() const noexcept { return m_refs; }

private:
    uint32_t m_refs = 0;
};

class ObjectTransform : public RefCounter
{
public:
    // main data
    DirectX::XMFLOAT4X4& ModifyMat() noexcept { m_is_dirty = true; return m_obj2world; }
    const DirectX::XMFLOAT4X4& Obj2World() const noexcept { return m_obj2world; }

    // properties
    bool IsDirty() const noexcept { return m_is_dirty; }
    void Clean() noexcept { m_is_dirty = false; }
private:
    friend class Scene;
    ObjectTransform() {}

    DirectX::XMFLOAT4X4 m_obj2world;

    bool m_is_dirty = false;
};
using TransformID = typename packed_freelist<ObjectTransform>::id;


class StaticMesh : public RefCounter
{
public:
    D3D12_VERTEX_BUFFER_VIEW& VertexBufferView() noexcept { return m_vbv; }
    const D3D12_VERTEX_BUFFER_VIEW& VertexBufferView() const noexcept { return m_vbv; }

    D3D12_INDEX_BUFFER_VIEW& IndexBufferView() noexcept { return m_ibv; }
    const D3D12_INDEX_BUFFER_VIEW& IndexBufferView() const noexcept { return m_ibv; }

    const auto& Vertices() const noexcept { return m_vertices; }
    auto& Vertices() noexcept { return m_vertices; }
    const auto& Indices() const noexcept { return m_indices; }
    auto& Indices() noexcept { return m_indices; }

    D3D_PRIMITIVE_TOPOLOGY Topology() const noexcept { return m_topology; }
    D3D_PRIMITIVE_TOPOLOGY& Topology() noexcept { return m_topology; }

    bool IsLoaded() const noexcept { return m_is_loaded; }
    void Load( const D3D12_VERTEX_BUFFER_VIEW& vbv, const D3D12_INDEX_BUFFER_VIEW& ibvy ) noexcept;

    void SubscribeToLoad(const std::function<void(void)>& callback);

    void OnLoaded();

private:
    friend class Scene;
    StaticMesh( ) {}

    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;

    D3D12_VERTEX_BUFFER_VIEW m_vbv;
    D3D12_INDEX_BUFFER_VIEW m_ibv;
    D3D_PRIMITIVE_TOPOLOGY m_topology;
    bool m_is_loaded = false;

    std::vector<std::function<void(void)>> m_loaded_callbacks;
};
using StaticMeshID = typename packed_freelist<StaticMesh>::id;


class StaticSubmesh_Deprecated : public RefCounter
{
public:
    struct Data
    {
        uint32_t idx_cnt;
        uint32_t start_index_loc;
        int32_t base_vertex_loc;
    };

    // main data
    const Data& DrawArgs() const noexcept { return m_data; }
    Data& Modify() noexcept { m_is_dirty = true; return m_data; }

    StaticMeshID GetMesh() const noexcept { return m_mesh_id; }

    ID3D12Resource* GetBLAS() const noexcept { return m_rt_blas.Get(); }
    void SetBLAS(ComPtr<ID3D12Resource> blas) { m_rt_blas = std::move(blas); }

    // properties
    DirectX::BoundingBox& Box() noexcept { return m_box; }
    const DirectX::BoundingBox& Box() const noexcept { return m_box; }

    DirectX::XMFLOAT2& MaxInverseUVDensity() noexcept { return m_max_inv_uv_density; }
    const DirectX::XMFLOAT2& MaxInverseUVDensity() const noexcept { return m_max_inv_uv_density; }

    bool IsDirty() const noexcept { return m_is_dirty; }
    void Clean() noexcept { m_is_dirty = false; }

private:
    friend class Scene;
    StaticSubmesh_Deprecated( StaticMeshID mesh_id ) : m_mesh_id( mesh_id ), m_data{ 0, 0, 0 } {}
    StaticMeshID& Mesh() noexcept { return m_mesh_id; }

    Data m_data;
    StaticMeshID m_mesh_id;

    ComPtr<ID3D12Resource> m_rt_blas;

    DirectX::BoundingBox m_box;
    DirectX::XMFLOAT2 m_max_inv_uv_density; // for mip streaming
    bool m_is_dirty = false;
};
using StaticSubmeshID = typename packed_freelist<StaticSubmesh_Deprecated>::id;


class Texture : public RefCounter
{
public:
    // main data
    D3D12_CPU_DESCRIPTOR_HANDLE& ModifyStagingSRV() noexcept { m_is_dirty = true; return m_staging_srv; }
    D3D12_CPU_DESCRIPTOR_HANDLE StagingSRV() const noexcept { return m_staging_srv; }

    // properties
    DirectX::XMFLOAT2& MaxPixelsPerUV() noexcept { return m_max_pixels_per_uv; }
    const DirectX::XMFLOAT2& MaxPixelsPerUV() const noexcept { return m_max_pixels_per_uv; }

    bool IsDirty() const noexcept { return m_is_dirty; }
    void Clean() noexcept { m_is_dirty = false; }

    bool IsLoaded() const noexcept { return m_is_loaded; }
    void Load( D3D12_CPU_DESCRIPTOR_HANDLE descriptor ) noexcept { ModifyStagingSRV() = descriptor; m_is_loaded = true; }

private:
    friend class Scene;
    Texture() {}

    D3D12_CPU_DESCRIPTOR_HANDLE m_staging_srv;
    DirectX::XMFLOAT2 m_max_pixels_per_uv = DirectX::XMFLOAT2( 0, 0 ); // for mip streaming
    bool m_is_dirty = false;
    bool m_is_loaded = false;
};
using TextureID = typename packed_freelist<Texture>::id;


class Cubemap : public RefCounter
{
public:
    // main data
    D3D12_CPU_DESCRIPTOR_HANDLE& ModifyStagingSRV() noexcept { m_is_dirty = true; return m_staging_srv; }
    D3D12_CPU_DESCRIPTOR_HANDLE StagingSRV() const noexcept { return m_staging_srv; }

    // properties
    bool IsDirty() const noexcept { return m_is_dirty; }
    void Clean() noexcept { m_is_dirty = false; }

    bool IsLoaded() const noexcept { return m_is_loaded; }
    void Load( D3D12_CPU_DESCRIPTOR_HANDLE descriptor ) noexcept { ModifyStagingSRV() = descriptor; m_is_loaded = true; }

private:
    friend class Scene;
    Cubemap() {}

    D3D12_CPU_DESCRIPTOR_HANDLE m_staging_srv;
    bool m_is_dirty = false;
    bool m_is_loaded = false;
};
using CubemapID = typename packed_freelist<Cubemap>::id;


class MaterialPBR : public RefCounter, public IRenderMaterial
{
public:
    struct TextureIds
    {
        TextureID base_color;
        TextureID normal;
        TextureID specular;
        TextureID preintegrated_brdf;
    };

    struct Data
    {
        DirectX::XMFLOAT4X4 transform;
		DirectX::XMFLOAT4 albedo_color;
        DirectX::XMFLOAT3 diffuse_fresnel;
    };

    // main data
    const TextureIds& Textures() const noexcept { return m_textures; }
    const Data& GetData() const noexcept { return m_data; }
    Data& Modify() noexcept { m_is_dirty = true; return m_data; }

    // properties
    // descriptor table with 3 entries. valid only if all textures above are loaded
    D3D12_GPU_DESCRIPTOR_HANDLE& DescriptorTable() noexcept { return m_desc_table; }
    const D3D12_GPU_DESCRIPTOR_HANDLE& DescriptorTable() const noexcept { return m_desc_table; }

    // material cb
    D3D12_GPU_VIRTUAL_ADDRESS& GPUConstantBuffer() noexcept { return m_material_cb; }
    const D3D12_GPU_VIRTUAL_ADDRESS& GPUConstantBuffer() const noexcept { return m_material_cb; }

    bool IsDirty() const noexcept { return m_is_dirty; }
    void Clean() noexcept { m_is_dirty = false; }

private:
    friend class Scene;
    MaterialPBR() {}
    TextureIds& Textures() noexcept { return m_textures; }

    TextureIds m_textures;

    Data m_data;

    D3D12_GPU_DESCRIPTOR_HANDLE m_desc_table;
    D3D12_GPU_VIRTUAL_ADDRESS m_material_cb;

    bool m_is_dirty = false;

    // Inherited via IRenderMaterial
    virtual std::pair<uint64_t, uint64_t> GetPipelineStateID( FramegraphTechnique technique ) const override;
    virtual bool BindDataToPipeline( FramegraphTechnique technique, uint64_t item_id, IGraphicsCommandList& cmd_list ) const override;
};
using MaterialID = typename packed_freelist<MaterialPBR>::id;


class EnvironmentMap : public RefCounter
{
public:
    CubemapID GetMap() const noexcept { return m_cubemap; }

    TransformID GetTransform() const noexcept { return m_tf; }

    float GetRadianceFactor() const noexcept { return m_radiance_factor; }
    float& SetRadianceFactor() noexcept { return m_radiance_factor; }

    D3D12_GPU_DESCRIPTOR_HANDLE GetSRV() const noexcept { return m_srv; }
    D3D12_GPU_DESCRIPTOR_HANDLE& SetSRV() noexcept { return m_srv; }

private:
    friend class Scene;
    EnvironmentMap() {}

    CubemapID& Map() noexcept { return m_cubemap; }
    TransformID& Transform() noexcept { return m_tf; }

    CubemapID m_cubemap = CubemapID::nullid;
    TransformID m_tf;
    D3D12_GPU_DESCRIPTOR_HANDLE m_srv;
    float m_radiance_factor = 1.0f;
};
using EnvMapID = typename packed_freelist<EnvironmentMap>::id;


class Light
{
public:
    enum class LightType
    {
        Parallel,
        Point,
        Spotlight
    };

    struct Data
    {
        LightType type;
        DirectX::XMFLOAT3 strength; // spectral irradiance, watt/sq.meter
        DirectX::XMFLOAT3 origin; // point and spotlight
        DirectX::XMFLOAT3 dir; // spotlight and parallel, direction TO the light source
        float falloff_start; // point and spotlight, meters
        float falloff_end; // point and spotlight, meters
        float spot_power; // spotlight only
        float angle; // directional
    };

    struct Shadow
    {
        size_t sm_size; // must be power of 2, base resolution for cascade
        float orthogonal_ws_height; // shadow map pass for parallel light places the light camera above the main camera.
                                    // This parameter indicates how high will it be placed. It depends mostly on the scene as a whole
        uint32_t num_cascades = 1; // point and spotlight csm is not supported
    };

    const Data& GetData() const noexcept { return m_data; }
    Data& ModifyData() noexcept { return m_data; }

    const std::optional<Shadow>& GetShadow() const noexcept { return m_sm; }
    std::optional<Shadow>& ModifyShadow() noexcept { return m_sm; }

    // has value if this light casts shadow in this frame
    // maps 3d points to shadow map uv and depth 
    using ShadowMatrices = boost::container::small_vector<DirectX::XMMATRIX, 1>;
    const ShadowMatrices& GetShadowMatrices() const noexcept { return m_shadow_matrices; };
    ShadowMatrices& SetShadowMatrices() noexcept { return m_shadow_matrices; };

    bool& IsEnabled() noexcept { return m_is_enabled; }
    const bool& IsEnabled() const noexcept { return m_is_enabled; }

private:
    Data m_data = {}; 
    ShadowMatrices m_shadow_matrices;
    std::optional<Shadow> m_sm;

    bool m_is_enabled = true;
};
using LightID = typename packed_freelist<Light>::id;


