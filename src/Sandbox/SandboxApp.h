#pragma once

#include "StdAfx.h"

#include "Editor.h"

#include <Engine/EngineApp.h>
#include <Engine/Assets.h>
#include <Engine/RHIUtils.h>
#include <Engine/Scene.h>
#include <Engine/LevelObjects.h>
#include <ImguiBackend/ImguiBackend.h>

SE_LOG_CATEGORY( Sandbox );

class SandboxApp : public EngineApp
{
private:
	std::string m_current_level_path = std::string( "#engine/Levels/Default.sel" );

	std::unique_ptr<World> m_world;

	std::unique_ptr<Scene> m_scene;
	std::unique_ptr<SceneView> m_scene_view;

	// GUI state
	bool m_show_imgui_demo = false;
	bool m_show_world_outliner = false;
	float m_fov_degrees = 45.0f;

	std::vector<std::unique_ptr<LevelObject>> m_level_objects;

	std::unique_ptr<Editor> m_editor;

public:
	SandboxApp();
	~SandboxApp();

private:
	virtual void ParseCommandLineDerived( int argc, char** argv ) override;

	virtual void OnInit() override;

	virtual void OnCleanup() override;

	virtual void OnDrawFrame( Rendergraph& framegraph, RHICommandList* ui_cmd_list ) override;

	virtual void OnUpdate() override;

	virtual void OnSwapChainRecreated() override;

	virtual const char* GetMainWindowName() const override { return "SnowEngine Sandbox"; }
	virtual const char* GetAppName() const override { return "SnowEngineSandbox"; }

	void UpdateGui();

	void UpdateScene();

	bool SaveLevel( const char* filepath ) const;
	bool OpenLevel( const char* filepath );
};
