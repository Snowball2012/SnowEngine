#include "stdafx.h"

#include "SceneManager.h"


SceneManager::SceneManager()
	: m_scene_view( &m_scene )
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