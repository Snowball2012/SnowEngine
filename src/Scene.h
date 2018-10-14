#pragma once

#include "utils/packed_freelist.h"

#include "SceneItems.h"

class Scene
{
public:
	// Transforms
	TransformID AddTransform( ) noexcept;
	bool RemoveTransform( TransformID id ) noexcept; // returns true if remove was successful or object with this id no longer exists. Can fail if the object still has refs from other scene components.
	// read-only
	const auto& AllTransforms() const noexcept { return m_obj_tfs; }
	auto TransformSpan() const noexcept { return m_obj_tfs.get_elems(); }
	// for element modification
	auto TransformSpan() noexcept { return m_obj_tfs.get_elems(); }
	ObjectTransform* TryModifyTransform( TransformID id ) noexcept; // returns nullptr if object no longer exists


	// Static meshes
	StaticMeshID AddStaticMesh( ) noexcept;
	bool RemoveStaticMesh( StaticMeshID id ) noexcept; // returns true if remove was successful or object with this id no longer exists. Can fail if the object still has refs from other scene components.
	// read-only
	const auto& AllStaticMeshes() const noexcept { return m_static_meshes; }
	auto StaticMeshSpan() const noexcept { return m_static_meshes.get_elems(); }
	// for element modification
	auto StaticMeshSpan() noexcept { return m_static_meshes.get_elems(); }
	StaticMesh* TryModifyStaticMesh( StaticMeshID id ) noexcept; // returns nullptr if object no longer exists


	// Static submeshes
	StaticSubmeshID AddStaticSubmesh( StaticMeshID mesh_id );
	bool RemoveStaticSubmesh( StaticSubmeshID id ) noexcept; // returns true if remove was successful or object with this id no longer exists. Can fail if the object still has refs from other scene components.
	// read-only
	const auto& AllStaticSubmeshes() const noexcept { return m_static_submeshes; }
	auto StaticSubmeshSpan() const noexcept { return m_static_submeshes.get_elems(); }
	// for element modification
	auto StaticSubmeshSpan() noexcept { return m_static_submeshes.get_elems(); }
	StaticSubmesh* TryModifyStaticSubmesh( StaticSubmeshID id ) noexcept; // returns nullptr if object no longer exists


	// Textures
	TextureID AddTexture() noexcept;
	bool RemoveTexture( TextureID id ) noexcept; // returns true if remove was successful or object with this id no longer exists. Can fail if the object still has refs from other scene components.
	// read-only
	const auto& AllTextures() const noexcept { return m_textures; }
	auto TextureSpan() const noexcept { return m_textures.get_elems(); }
	// for element modification
	auto TextureSpan() noexcept { return m_textures.get_elems(); }
	Texture* TryModifyTexture( TextureID id ) noexcept; // returns nullptr if object no longer exists


	// Materials
	MaterialID AddMaterial( const MaterialPBR::TextureIds& textures );
	bool RemoveMaterial( MaterialID id ) noexcept; // returns true if remove was successful or object with this id no longer exists. Can fail if the object still has refs from other scene components.
	// read-only
	const auto& AllMaterials() const noexcept { return m_materials; }
	auto MaterialSpan() const noexcept { return m_materials.get_elems(); }
	// for element modification
	auto MaterialSpan() noexcept { return m_materials.get_elems(); }
	MaterialPBR* TryModifyMaterial( MaterialID id ) noexcept; // returns nullptr if object no longer exists


	// Static mesh instances
	MeshInstanceID AddStaticMeshInstance( TransformID tf, StaticSubmeshID submesh, MaterialID material );
	bool RemoveStaticMeshInstance( MeshInstanceID id ) noexcept; // returns true if remove was successful or object with this id no longer exists. Can fail if the object still has refs from other scene components.
	// read-only
	const auto& AllStaticMeshInstances() const noexcept { return m_static_mesh_instances; }
	auto StaticMeshInstanceSpan() const noexcept { return m_static_mesh_instances.get_elems(); }
	// for element modification
	auto StaticMeshInstanceSpan() noexcept { return m_static_mesh_instances.get_elems(); }
	StaticMeshInstance* TryModifyStaticMeshInstance( MeshInstanceID id ) noexcept; // returns nullptr if object no longer exists

private:
	template<typename ID>
	struct ID2Obj;

	template<typename ID>
	using freelist_from_id = packed_freelist<typename ID2Obj<ID>::type>;

	template<typename ID>
	freelist_from_id<ID>& GetStorage() noexcept;

	// returns true if remove was successful or object with this id no longer exists.
	// can fail if the object still has refs from other scene components
	template<typename ID>
	bool Remove( ID obj ) noexcept;

	packed_freelist<ObjectTransform> m_obj_tfs;
	packed_freelist<StaticMesh> m_static_meshes;
	packed_freelist<StaticSubmesh> m_static_submeshes;
	packed_freelist<Texture> m_textures;
	packed_freelist<MaterialPBR> m_materials;
	packed_freelist<StaticMeshInstance> m_static_mesh_instances;
};
