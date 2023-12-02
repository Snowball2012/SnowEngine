#pragma once

#include "StdAfx.h"

#include "Editor.h"

#include <Engine/EngineApp.h>
#include <Engine/Assets.h>
#include <Engine/RHIUtils.h>
#include <Engine/Scene.h>
#include <Engine/LevelObjects.h>
#include <ImguiBackend/ImguiBackend.h>

class SandboxApp : public EngineApp
{
private:
	std::string m_current_level_path = std::string( "#engine/Levels/Default.sel" );

	// GUI state
	bool m_show_imgui_demo = false;
	bool m_show_world_outliner = false;

	std::unique_ptr<LevelEditor> m_editor;

	decltype( std::chrono::high_resolution_clock::now() ) m_last_tick_time;

public:
	SandboxApp();
	~SandboxApp();

private:
	virtual void ParseCommandLineDerived( int argc, char** argv ) override;

	virtual void OnInit() override;

	virtual void OnCleanup() override;

	virtual void OnDrawFrame( Rendergraph& framegraph, const AppGPUReadbackData& readback_data, RHICommandList* ui_cmd_list ) override;

	virtual void OnUpdate() override;

	virtual void OnSwapChainRecreated() override;

	virtual void OnFrameRenderFinish( const AppGPUReadbackData& data ) override;
	virtual void CreateReadbackData( AppGPUReadbackData& data ) override;

	virtual const char* GetMainWindowName() const override { return "SnowEngine Sandbox"; }
	virtual const char* GetAppName() const override { return "SnowEngineSandbox"; }

	void UpdateGui();
};
