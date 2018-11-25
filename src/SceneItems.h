#pragma once

#include <d3d12.h>
#include <DirectXCollision.h>
#include <DirectXMath.h>

#include "utils/packed_freelist.h"

class Scene;

struct Vertex
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 normal;
	DirectX::XMFLOAT2 uv;
};

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
	D3D12_GPU_VIRTUAL_ADDRESS& GPUView() noexcept { return m_gpu; }
	const D3D12_GPU_VIRTUAL_ADDRESS& GPUView() const noexcept { return m_gpu; }
	bool IsDirty() const noexcept { return m_is_dirty; }
	void Clean() noexcept { m_is_dirty = false; }
private:
	friend class Scene;
	ObjectTransform() {}

	DirectX::XMFLOAT4X4 m_obj2world;
	D3D12_GPU_VIRTUAL_ADDRESS m_gpu;

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

private:
	friend class Scene;
	StaticMesh( ) {}

	std::vector<Vertex> m_vertices;
	std::vector<uint32_t> m_indices;

	D3D12_VERTEX_BUFFER_VIEW m_vbv;
	D3D12_INDEX_BUFFER_VIEW m_ibv;
	D3D_PRIMITIVE_TOPOLOGY m_topology;
	bool m_is_loaded = false;
};
using StaticMeshID = typename packed_freelist<StaticMesh>::id;


class StaticSubmesh : public RefCounter
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

	// properties
	DirectX::BoundingBox& Box() noexcept { return m_box; }
	const DirectX::BoundingBox& Box() const noexcept { return m_box; }

	DirectX::XMFLOAT2& MaxInverseUVDensity() noexcept { return m_max_inv_uv_density; }
	const DirectX::XMFLOAT2& MaxInverseUVDensity() const noexcept { return m_max_inv_uv_density; }

	bool IsDirty() const noexcept { return m_is_dirty; }
	void Clean() noexcept { m_is_dirty = false; }

private:
	friend class Scene;
	StaticSubmesh( StaticMeshID mesh_id ) : m_mesh_id( mesh_id ), m_data{ 0, 0, 0 } {}
	StaticMeshID& Mesh() noexcept { return m_mesh_id; }

	Data m_data;
	StaticMeshID m_mesh_id;

	DirectX::BoundingBox m_box;
	DirectX::XMFLOAT2 m_max_inv_uv_density; // for mip streaming
	bool m_is_dirty = false;
};
using StaticSubmeshID = typename packed_freelist<StaticSubmesh>::id;


class Texture : public RefCounter
{
public:
	// main data
	D3D12_CPU_DESCRIPTOR_HANDLE& ModifyStagingSRV() noexcept { m_is_dirty = true; return m_staging_srv; }
	D3D12_CPU_DESCRIPTOR_HANDLE StagingSRV() const noexcept { return m_staging_srv; }

	// properties
	DirectX::XMFLOAT2& MaxPixelsPerUV() noexcept { return m_max_pixels_per_uv; }

	bool IsDirty() const noexcept { return m_is_dirty; }
	void Clean() noexcept { m_is_dirty = false; }

	bool IsLoaded() const noexcept { return m_is_loaded; }
	void Load( D3D12_CPU_DESCRIPTOR_HANDLE descriptor ) noexcept { ModifyStagingSRV() = descriptor; m_is_loaded = true; }

private:
	friend class Scene;
	Texture() {}

	D3D12_CPU_DESCRIPTOR_HANDLE m_staging_srv;
	DirectX::XMFLOAT2 m_max_pixels_per_uv; // for mip streaming
	bool m_is_dirty = false;
	bool m_is_loaded = false;
};
using TextureID = typename packed_freelist<Texture>::id;


class MaterialPBR : public RefCounter
{
public:
	struct TextureIds
	{
		TextureID base_color;
		TextureID normal;
		TextureID specular;
	};

	struct Data
	{
		DirectX::XMFLOAT4X4 transform;
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
};
using MaterialID = typename packed_freelist<MaterialPBR>::id;


class StaticMeshInstance
{
public:
	TransformID GetTransform() const noexcept { return m_transform; }
	MaterialID Material() const noexcept { return m_material; }
	StaticSubmeshID Submesh() const noexcept { return m_submesh; }

	bool HasShadow() const noexcept { return m_has_shadow; }
	bool& HasShadow() noexcept { return m_has_shadow; }

	bool IsEnabled() const noexcept { return m_is_enabled; }
	bool& IsEnabled() noexcept { return m_is_enabled; }

private:
	friend class Scene;
	StaticMeshInstance() {}
	TransformID& Transform() noexcept { return m_transform; }
	MaterialID& Material() noexcept { return m_material; }
	StaticSubmeshID& Submesh() noexcept { return m_submesh; }

	TransformID m_transform;
	MaterialID m_material;
	StaticSubmeshID m_submesh;

	bool m_has_shadow = false;
	bool m_is_enabled = true;
};
using MeshInstanceID = typename packed_freelist<StaticMeshInstance>::id;


class Camera
{
public:
	enum class Type
	{
		Perspective,
		Orthographic
	};
	struct Data
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT3 dir;
		DirectX::XMFLOAT3 up;
		float aspect_ratio;
		union
		{
			float fov_y;
			float height;
		};
		float near_plane;
		float far_plane;
		Type type;
	};
	const Data& GetData() const noexcept { return m_data; }
	Data& ModifyData() noexcept { return m_data; }

private:
	Data m_data;
};
using CameraID = typename packed_freelist<Camera>::id;


class SceneLight
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
	};

	struct Shadow
	{
		size_t sm_size; // must be power of 2, base resolution for cascade
		float orthogonal_ws_height; // shadow map pass for parallel light places the light camera above the main camera.
		                            // This parameter indicates how high will it be placed. It depends mostly on the scene as a whole
		float most_detailed_cascade_ws_halfwidth; // width of the most detailed slice in world space units
		// todo: cascade properties
	};

	const Data& GetData() const noexcept { return m_data; }
	Data& ModifyData() noexcept { return m_data; }

	const std::optional<Shadow>& GetShadow() const noexcept { return m_sm; }
	std::optional<Shadow>& ModifyShadow() noexcept { return m_sm; }

	// has value if this light casts shadow in this frame
	// maps 3d points to shadow map uv and depth 
	const std::optional<DirectX::XMMATRIX>& ShadowMatrix() const noexcept { return m_shadow_matrix; };
	std::optional<DirectX::XMMATRIX>& ShadowMatrix() noexcept { return m_shadow_matrix; };

	bool& IsEnabled() noexcept { return m_is_enabled; }
	const bool& IsEnabled() const noexcept { return m_is_enabled; }

private:
	Data m_data; 
	std::optional<DirectX::XMMATRIX> m_shadow_matrix;
	std::optional<Shadow> m_sm;

	bool m_is_enabled = true;
};
using LightID = typename packed_freelist<SceneLight>::id;