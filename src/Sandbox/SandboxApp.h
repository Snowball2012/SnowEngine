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
	// pipeline
	RHIDescriptorSetLayoutPtr m_dsl_fsquad = nullptr;
	RHIShaderBindingLayoutPtr m_shader_bindings_layout_fsquad = nullptr;
	RHIGraphicsPipelinePtr m_draw_fullscreen_quad_pipeline = nullptr;

	// resources
	RHISamplerPtr m_point_sampler = nullptr;

	// assets
	MeshAssetPtr m_demo_mesh = nullptr;
	std::string m_demo_mesh_asset_path = std::string( "#engine/Meshes/Cube.sea" );

	// descriptors
	std::vector<RHIDescriptorSetPtr> m_fsquad_descsets;

	std::unique_ptr<World> m_world;

	std::unique_ptr<Scene> m_scene;
	std::unique_ptr<SceneView> m_scene_view;

	// GUI state
	std::string m_new_entity_name;
	bool m_show_imgui_demo = false;
	bool m_show_world_outliner = false;
	float m_fov_degrees = 45.0f;

	LevelObject* m_demo_object = nullptr;

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

	void CleanupPipeline();

	void CreateDescriptorSetLayout();

	void CreateDescriptorSets();

	void CreateTextureSampler();

	void CreateFullscreenQuadPipeline();

	void UpdateScene();

	bool SaveLevel( const char* filepath ) const;
};
