#include "stdafx.h"

#include "Scene.h"

// StaticMesh

void StaticMesh::Load( const D3D12_VERTEX_BUFFER_VIEW& vbv, const D3D12_INDEX_BUFFER_VIEW& ibv, D3D_PRIMITIVE_TOPOLOGY topology ) noexcept
{
	m_vbv = vbv;
	m_ibv = ibv;
	m_topology = topology;
	m_is_loaded = true;
}

// Scene

// template helpers
template<> struct Scene::ID2Obj<TransformID>
{
	using type = ObjectTransform;
};

template<> struct Scene::ID2Obj<StaticMeshID>
{
	using type = StaticMesh;
};

template<> struct Scene::ID2Obj<StaticSubmeshID>
{
	using type = StaticSubmesh;
};

template<> Scene::freelist_from_id<TransformID>& Scene::GetStorage<TransformID>() noexcept
{
	return m_obj_tfs;
}

template<> Scene::freelist_from_id<StaticMeshID>& Scene::GetStorage<StaticMeshID>() noexcept
{
	return m_static_meshes;
}

template<> Scene::freelist_from_id<StaticSubmeshID>& Scene::GetStorage<StaticSubmeshID>() noexcept
{
	return m_static_submeshes;
}

// generic methods
template<typename IDType>
bool Scene::Remove( IDType obj_id ) noexcept
{
	auto* obj = GetStorage<IDType>().try_get( obj_id );
	if ( ! obj )
		return true;

	if ( obj->GetRefCount() != 0 )
		return false;

	GetStorage<IDType>().erase( obj_id );
	return true;
}
template bool Scene::Remove<TransformID>( TransformID ) noexcept;
template bool Scene::Remove<StaticMeshID>( StaticMeshID ) noexcept;
template bool Scene::Remove<StaticSubmeshID>( StaticSubmeshID ) noexcept;


TransformID Scene::AddTransform( bool is_dynamic )
{
	NOTIMPL;
}

ObjectTransform* Scene::TryModifyTransform( TransformID id ) noexcept
{
	return m_obj_tfs.try_get( id );
}

StaticMeshID Scene::AddStaticMesh()
{
	NOTIMPL;
}

StaticMesh* Scene::TryModifyStaticMesh( StaticMeshID id ) noexcept
{
	return m_static_meshes.try_get( id );
}

StaticSubmeshID Scene::AddStaticSubmesh()
{
	NOTIMPL;
}

StaticSubmesh* Scene::TryModifyStaticSubmesh( StaticSubmeshID id ) noexcept
{
	return m_static_submeshes.try_get( id );
}
