#pragma once

#include "Scene.h"
#include "RenderData.h"

class SceneManager;
class StaticMeshManager;

// update, remove and add methods for scene
class SceneClientView
{
public:
	SceneClientView( Scene* scene, StaticMeshManager* smm ) : m_scene( scene ), m_static_mesh_manager( smm ) {}

	const Scene& GetROScene() const noexcept { return *m_scene; }

	StaticMeshID LoadStaticMesh( std::string name, const span<const Vertex>& vertices, const span<const uint32_t>& indices );

private:
	Scene* m_scene;
	StaticMeshManager* m_static_mesh_manager;
};

using SceneCopyOp = uint32_t;

// Manages scene lifetime, binds the scene to a render pipeline
class SceneManager
{
public:
	SceneManager( StaticMeshManager* smm );

	const SceneClientView& GetScene() const noexcept;
	SceneClientView& GetScene() noexcept;

	void UpdatePipelineBindings();

	template<typename PipelineT>
	void BindToPipeline( PipelineT* pipeline );

private:

	Scene m_scene;
	SceneClientView m_scene_view;
};

#include "SceneManager.hpp"