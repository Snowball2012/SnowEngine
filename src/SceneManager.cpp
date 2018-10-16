#include "stdafx.h"

#include "SceneManager.h"

#include "StaticMeshManager.h"

StaticMeshID SceneClientView::LoadStaticMesh( std::string name, const span<const Vertex>& vertices, const span<const uint32_t>& indices )
{
	NOTIMPL;
}

SceneManager::SceneManager( StaticMeshManager* smm )
	: m_scene_view( &m_scene, smm )
{
}

const SceneClientView& SceneManager::GetScene() const noexcept
{
	return m_scene_view;
}

SceneClientView& SceneManager::GetScene() noexcept
{
	return m_scene_view;
}


void SceneManager::UpdatePipelineBindings()
{
	NOTIMPL;
}
