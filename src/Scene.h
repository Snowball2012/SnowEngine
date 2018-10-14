#pragma once

#include "utils/packed_freelist.h"

#include <d3d12.h>
#include <DirectXCollision.h>
#include <DirectXMath.h>

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

	DirectX::XMFLOAT4X4 m_obj2world;
	D3D12_GPU_VIRTUAL_ADDRESS m_gpu;
	bool m_is_dirty = false;
};
using TransformID = packed_freelist<ObjectTransform>::id;


class StaticMesh : public RefCounter
{
public:
	D3D12_VERTEX_BUFFER_VIEW& VertexBufferView() noexcept { return m_vbv; }
	const D3D12_VERTEX_BUFFER_VIEW& VertexBufferView() const noexcept { return m_vbv; }

	D3D12_VERTEX_BUFFER_VIEW& IndexBufferView() noexcept { return m_vbv; }
	const D3D12_VERTEX_BUFFER_VIEW& IndexBufferView() const noexcept { return m_vbv; }

	D3D_PRIMITIVE_TOPOLOGY Topology() const noexcept { return m_topology; }

	bool IsLoaded() const noexcept { return m_is_loaded; }
	void Load( const D3D12_VERTEX_BUFFER_VIEW& vbv, const D3D12_INDEX_BUFFER_VIEW& ibv, D3D_PRIMITIVE_TOPOLOGY topology ) noexcept;

private:
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
		StaticMeshID mesh_id;
		uint32_t idx_cnt;
		uint32_t start_index_loc;
		int32_t base_vertex_loc;
	};

	// main data
	const Data& DrawArgs() const noexcept { return m_data; }
	Data& Modify() noexcept { m_is_dirty = true; return m_data; }

	// properties
	DirectX::BoundingBox& Box() noexcept { return m_box; }
	const DirectX::BoundingBox& Box() const noexcept { return m_box; }

	DirectX::XMFLOAT2& MaxInverseUVDensity() noexcept { return m_max_inv_uv_density; }

	bool IsDirty() const noexcept { return m_is_dirty; }
	void Clean() noexcept { m_is_dirty = false; }
private:
	Data m_data;

	DirectX::BoundingBox m_box;
	DirectX::XMFLOAT2 m_max_inv_uv_density; // for mip streaming
	bool m_is_dirty = false;
};
using StaticSubmeshID = packed_freelist<StaticSubmesh>::id;


class Scene
{
public:

	// General methods

	// returns true if remove was successful or object with this id no longer exists.
	// can fail if the transform still has refs from other scene components
	template<typename ID>
	bool Remove( ID obj ) noexcept;


	// Transforms
	TransformID AddTransform( bool is_dynamic );
	const auto& AllTransforms() const noexcept { return m_obj_tfs; }
	ObjectTransform* TryModifyTransform( TransformID id ) noexcept; // returns nullptr if object no longer exists


	// Static meshes
	StaticMeshID AddStaticMesh( );
	const auto& AllStaticMeshes() const noexcept { return m_static_meshes; }
	StaticMesh* TryModifyStaticMesh( StaticMeshID id ) noexcept; // returns nullptr if object no longer exists


	// Static submeshes
	StaticSubmeshID AddStaticSubmesh();
	const auto& AllStaticSubmeshes() const noexcept { return m_static_meshes; }
	StaticSubmesh* TryModifyStaticSubmesh( StaticSubmeshID id ) noexcept; // returns nullptr if object no longer exists

private:

	template<typename ID>
	struct ID2Obj;

	template<typename ID>
	using freelist_from_id = packed_freelist<typename ID2Obj<ID>::type>;

	template<typename ID>
	freelist_from_id<ID>& GetStorage() noexcept;

	packed_freelist<ObjectTransform> m_obj_tfs;
	packed_freelist<StaticMesh> m_static_meshes;
	packed_freelist<StaticSubmesh> m_static_submeshes;
};
