#pragma once

#include "Scene.h"

class SceneManager;

// update, remove and add methods for scene
class SceneClientView
{
public:
	SceneClientView( Scene* scene ) : m_scene( scene ) {}

	const Scene& GetROScene() const noexcept { return *m_scene; }
private:
	Scene* m_scene;
};

class SceneManager
{
public:
	SceneManager();

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