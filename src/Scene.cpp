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
template<> struct Scene::ID2Obj<Scene::TransformID>
{
	using type = ObjectTransform;
};

template<> struct Scene::ID2Obj<Scene::StaticMeshID>
{
	using type = StaticMesh;
};

template<> Scene::freelist_from_id<Scene::TransformID>& Scene::GetStorage<Scene::TransformID>() noexcept
{
	return m_obj_tfs;
}

template<> Scene::freelist_from_id<Scene::StaticMeshID>& Scene::GetStorage<Scene::StaticMeshID>() noexcept
{
	return m_obj_tfs;
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
template bool Scene::Remove<Scene::TransformID>( Scene::TransformID ) noexcept;
template bool Scene::Remove<Scene::StaticMeshID>( Scene::StaticMeshID ) noexcept;


Scene::TransformID Scene::AddTransform( bool is_dynamic )
{
	NOTIMPL;
}

ObjectTransform* Scene::TryModifyTransform( TransformID id ) noexcept
{
	return m_obj_tfs.try_get( id );
}

Scene::StaticMeshID Scene::AddStaticMesh()
{
	NOTIMPL;
}

StaticMesh* Scene::TryModifyStaticMesh( StaticMeshID id ) noexcept
{
	return m_static_meshes.try_get( id );
}
