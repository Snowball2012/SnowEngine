#pragma once

#include <d3d12.h>
#include <DirectXCollision.h>
#include <DirectXMath.h>

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
using TransformID = packed_freelist<ObjectTransform>::id;


class StaticMesh : public RefCounter
{
public:
	D3D12_VERTEX_BUFFER_VIEW & VertexBufferView() noexcept { return m_vbv; }
	const D3D12_VERTEX_BUFFER_VIEW& VertexBufferView() const noexcept { return m_vbv; }

	D3D12_VERTEX_BUFFER_VIEW& IndexBufferView() noexcept { return m_vbv; }
	const D3D12_VERTEX_BUFFER_VIEW& IndexBufferView() const noexcept { return m_vbv; }

	D3D_PRIMITIVE_TOPOLOGY Topology() const noexcept { return m_topology; }

	bool IsLoaded() const noexcept { return m_is_loaded; }
	void Load( const D3D12_VERTEX_BUFFER_VIEW& vbv, const D3D12_INDEX_BUFFER_VIEW& ibv, D3D_PRIMITIVE_TOPOLOGY topology ) noexcept;

private:
	friend class Scene;
	StaticMesh() {}

	D3D12_VERTEX_BUFFER_VIEW m_vbv;
	D3D12_INDEX_BUFFER_VIEW m_ibv;
	D3D_PRIMITIVE_TOPOLOGY m_topology;
	bool m_is_loaded = false;
};
using StaticMeshID = packed_freelist<StaticMesh>::id;


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

	bool IsDirty() const noexcept { return m_is_dirty; }
	void Clean() noexcept { m_is_dirty = false; }

private:
	friend class Scene;
	StaticSubmesh( StaticMeshID mesh_id ) : m_mesh_id( mesh_id ) {}
	StaticMeshID& Mesh() noexcept { return m_mesh_id; }

	Data m_data;
	StaticMeshID m_mesh_id;

	DirectX::BoundingBox m_box;
	DirectX::XMFLOAT2 m_max_inv_uv_density; // for mip streaming
	bool m_is_dirty = false;
};
using StaticSubmeshID = packed_freelist<StaticSubmesh>::id;


class Texture : public RefCounter
{
public:
	// main data
	D3D12_CPU_DESCRIPTOR_HANDLE& ModifyStagingSRV() noexcept { m_is_dirty = false; return m_staging_srv; }
	D3D12_CPU_DESCRIPTOR_HANDLE StagingSRV() const noexcept { return m_staging_srv; }

	// properties
	DirectX::XMFLOAT2& MaxNDCPerUV() noexcept { return m_max_ndc_per_uv; }

	bool IsDirty() const noexcept { return m_is_dirty; }
	void Clean() noexcept { m_is_dirty = false; }

	bool IsLoaded() const noexcept { return m_is_loaded; }
	void Load( D3D12_CPU_DESCRIPTOR_HANDLE descriptor ) noexcept { ModifyStagingSRV() = descriptor; m_is_loaded = true; }

private:
	friend class Scene;
	Texture() {}

	D3D12_CPU_DESCRIPTOR_HANDLE m_staging_srv;
	DirectX::XMFLOAT2 m_max_ndc_per_uv; // for mip streaming
	bool m_is_dirty = false;
	bool m_is_loaded = false;
};
using TextureID = packed_freelist<Texture>::id;


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

	bool IsDirty() const noexcept { return m_is_dirty; }
	void Clean() noexcept { m_is_dirty = false; }

private:
	friend class Scene;
	MaterialPBR() {}
	TextureIds& Textures() noexcept { return m_textures; }

	TextureIds m_textures;

	Data m_data;

	D3D12_GPU_DESCRIPTOR_HANDLE m_desc_table;

	bool m_is_dirty = false;
};
using MaterialID = packed_freelist<MaterialPBR>::id;


class StaticMeshInstance
{
public:
	TransformID Transform() const noexcept { return m_transform; }
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
using MeshInstanceID = packed_freelist<StaticMeshInstance>::id;

