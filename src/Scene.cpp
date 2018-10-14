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

template<> struct Scene::ID2Obj<TextureID>
{
	using type = Texture;
};

template<> struct Scene::ID2Obj<MaterialID>
{
	using type = MaterialPBR;
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

template<> Scene::freelist_from_id<TextureID>& Scene::GetStorage<TextureID>() noexcept
{
	return m_textures;
}

template<> Scene::freelist_from_id<MaterialID>& Scene::GetStorage<MaterialID>() noexcept
{
	return m_materials;
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


// Transforms

TransformID Scene::AddTransform( ) noexcept
{
	ObjectTransform tf;
	return m_obj_tfs.insert( std::move( tf ) );
}

bool Scene::RemoveTransform( TransformID id ) noexcept
{
	return Remove( id );
}

ObjectTransform* Scene::TryModifyTransform( TransformID id ) noexcept
{
	return m_obj_tfs.try_get( id );
}


// Static meshes

StaticMeshID Scene::AddStaticMesh() noexcept
{
	StaticMesh mesh;
	return m_static_meshes.insert( std::move( mesh ) );
}

bool Scene::RemoveStaticMesh( StaticMeshID id ) noexcept
{
	return Remove( id );
}

StaticMesh* Scene::TryModifyStaticMesh( StaticMeshID id ) noexcept
{
	return m_static_meshes.try_get( id );
}


// Submeshes
StaticSubmeshID Scene::AddStaticSubmesh( StaticMeshID mesh_id )
{
	StaticMesh* mesh = m_static_meshes.try_get( mesh_id );
	if ( ! mesh )
		throw SnowEngineException( "referenced mesh does not exist" );
	mesh->AddRef();

	StaticSubmesh submesh( mesh_id );
	return m_static_submeshes.insert( std::move( submesh ) );
}

bool Scene::RemoveStaticSubmesh( StaticSubmeshID id ) noexcept
{
	StaticSubmesh* submesh = m_static_submeshes.try_get( id );
	if ( ! submesh )
		return true;

	StaticMesh* mesh = m_static_meshes.try_get( submesh->GetMesh() );

	assert( mesh != nullptr ); // It's a strange situation indeed, but I don't see why we should fail here, since we are deleting this element anyway
	if ( mesh )
		mesh->ReleaseRef();

	return Remove( id );
}

StaticSubmesh* Scene::TryModifyStaticSubmesh( StaticSubmeshID id ) noexcept
{
	return m_static_submeshes.try_get( id );
}


// Textures

TextureID Scene::AddTexture() noexcept
{
	Texture texture;
	return m_textures.insert( std::move( texture ) );
}

bool Scene::RemoveTexture( TextureID id ) noexcept
{
	return Remove( id );
}

Texture* Scene::TryModifyTexture( TextureID id ) noexcept
{
	return m_textures.try_get( id );
}


// Materials

MaterialID Scene::AddMaterial( const MaterialPBR::TextureIds& textures )
{
	auto add_ref = [&]( TextureID id )
	{
		Texture* tex = m_textures.try_get( id );
		if ( ! tex )
			throw SnowEngineException( "referenced texture does not exist" );
		tex->AddRef();
	};

	for ( const auto& tex_id : { textures.base_color, textures.normal, textures.specular } )
		add_ref( tex_id );

	MaterialPBR material;
	material.Textures() = textures;
	return m_materials.insert( std::move( material ) );
}

bool Scene::RemoveMaterial( MaterialID id ) noexcept
{
	MaterialPBR* material = m_materials.try_get( id );
	if ( ! material )
		return true;

	auto release_ref = [&]( TextureID id )
	{
		Texture* tex = m_textures.try_get( id );
		assert( tex != nullptr );
		if ( tex )
			tex->ReleaseRef();
	};

	const auto& textures = material->Textures();
	for ( const auto& tex_id : { textures.base_color, textures.normal, textures.specular } )
		release_ref( tex_id );

	return Remove( id );
}

MaterialPBR* Scene::TryModifyMaterial( MaterialID id ) noexcept
{
	return m_materials.try_get( id );
}


// Static mesh instances

MeshInstanceID Scene::AddStaticMeshInstance( TransformID tf_id, StaticSubmeshID submesh_id, MaterialID material_id )
{
	ObjectTransform* tf = m_obj_tfs.try_get( tf_id );
	StaticSubmesh* submesh = m_static_submeshes.try_get( submesh_id );
	MaterialPBR* material = m_materials.try_get( material_id );
	if ( ! ( tf && submesh && material ) )
		throw SnowEngineException( "referenced objects do not exist" );

	tf->AddRef();
	submesh->AddRef();
	material->AddRef();

	StaticMeshInstance instance;
	instance.Material() = material_id;
	instance.Submesh() = submesh_id;
	instance.Transform() = tf_id;

	return m_static_mesh_instances.insert( instance );
}

bool Scene::RemoveStaticMeshInstance( MeshInstanceID id ) noexcept
{
	StaticMeshInstance* instance = m_static_mesh_instances.try_get( id );
	if ( ! instance )
		return true;

	ObjectTransform* tf = m_obj_tfs.try_get( instance->Transform() );
	StaticSubmesh* submesh = m_static_submeshes.try_get( instance->Submesh() );
	MaterialPBR* material = m_materials.try_get( instance->Material() );

	assert( tf && submesh && material );
	if ( tf )
		tf->ReleaseRef();
	if ( submesh )
		submesh->ReleaseRef();
	if ( material )
		material->ReleaseRef();

	m_static_mesh_instances.erase( id );
	return true;
}

StaticMeshInstance* Scene::TryModifyStaticMeshInstance( MeshInstanceID id ) noexcept
{
	return m_static_mesh_instances.try_get( id );
}
