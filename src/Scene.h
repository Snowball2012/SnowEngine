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


	// Cubemaps
	CubemapID AddCubemap() noexcept;
	bool RemoveCubemap( CubemapID id ) noexcept; // returns true if remove was successful or object with this id no longer exists. Can fail if the object still has refs from other scene components.
	// read-only
	const auto& AllCubemaps() const noexcept { return m_cubemaps; }
	auto CubemapSpan() const noexcept { return m_cubemaps.get_elems(); }
	// for element modification
	auto CubemapSpan() noexcept { return m_cubemaps.get_elems(); }
	Cubemap* TryModifyCubemap( CubemapID id ) noexcept; // returns nullptr if object no longer exists


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

	
	// Cameras
	CameraID AddCamera( ) noexcept;
	bool RemoveCamera( CameraID id ) noexcept; // returns true if remove was successful or object with this id no longer exists. Can fail if the object still has refs from other scene components.
	// read-only
	const auto& AllCameras() const noexcept { return m_cameras; }
	auto CameraSpan() const noexcept { return m_cameras.get_elems(); }
	// for element modification
	auto CameraSpan() noexcept { return m_cameras.get_elems(); }
	Camera* TryModifyCamera( CameraID id ) noexcept; // returns nullptr if object no longer exists


	// Lights
	LightID AddLight() noexcept;
	bool RemoveLight( LightID id ) noexcept; // returns true if remove was successful or object with this id no longer exists. Can fail if the object still has refs from other scene components.
	// read-only
	const auto& AllLights() const noexcept { return m_lights; }
	auto LightSpan() const noexcept { return m_lights.get_elems(); }
	// for element modification
	auto LightSpan() noexcept { return m_lights.get_elems(); }
	SceneLight* TryModifyLight( LightID id ) noexcept; // returns nullptr if object no longer exists


	// Enviriment maps
	EnvMapID AddEnviromentMap( CubemapID cubemap_id, TransformID tf_id );
	bool RemoveEnviromentMap( EnvMapID id ) noexcept; // returns true if remove was successful or object with this id no longer exists. Can fail if the object still has refs from other scene components.
	// read-only
	const auto& AllEnviromentMaps() const noexcept { return m_env_maps; }
	auto EnviromentMapSpan() const noexcept { return m_env_maps.get_elems(); }
	// for element modification
	auto EnviromentMapSpan() noexcept { return m_env_maps.get_elems(); }
	EnviromentMap* TryModifyEnvMap( EnvMapID id ) noexcept; // returns nullptr if object no longer exists

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
	packed_freelist<Cubemap> m_cubemaps;
	packed_freelist<MaterialPBR> m_materials;
	packed_freelist<StaticMeshInstance> m_static_mesh_instances;
	packed_freelist<Camera> m_cameras;
	packed_freelist<SceneLight> m_lights;
	packed_freelist<EnviromentMap> m_env_maps;
};
