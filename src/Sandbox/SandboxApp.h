#pragma once

#include "StdAfx.h"

#include <ecs/EntityContainer.h>

#include <Engine/EngineApp.h>
#include <Engine/Assets.h>
#include <Engine/RHIUtils.h>
#include <Engine/Scene.h>

#include "RenderResources.h"

SE_LOG_CATEGORY( Sandbox );

struct NameComponent
{
	std::string name;
};

struct TransformComponent
{
	Transform tf;
};

struct MeshInstanceComponent
{
	SceneMeshInstanceID scene_mesh_instance = SceneMeshInstanceID::nullid;
};

using World = EntityContainer<
	NameComponent,
	TransformComponent,
	MeshInstanceComponent
>;

class SandboxApp : public EngineApp
{
private:
	// pipeline
	RHIDescriptorSetLayoutPtr m_binding_table_layout = nullptr;
	RHIShaderBindingLayoutPtr m_shader_bindings_layout = nullptr;
	RHIGraphicsPipelinePtr m_rhi_graphics_pipeline = nullptr;
	RHIGraphicsPipelinePtr m_cube_graphics_pipeline = nullptr;

	RHIDescriptorSetLayoutPtr m_dsl_fsquad = nullptr;
	RHIShaderBindingLayoutPtr m_shader_bindings_layout_fsquad = nullptr;
	RHIGraphicsPipelinePtr m_draw_fullscreen_quad_pipeline = nullptr;

	RHIDescriptorSetLayoutPtr m_rt_dsl = nullptr;
	RHIShaderBindingLayoutPtr m_rt_layout = nullptr;
	RHIRaytracingPipelinePtr m_rt_pipeline = nullptr;

	// resources
	RHIBufferPtr m_vertex_buffer = nullptr;
	RHIBufferPtr m_index_buffer = nullptr;
	std::vector<RHIUploadBufferPtr> m_uniform_buffers;
	std::vector<RHIUniformBufferViewPtr> m_uniform_buffer_views;
	// images
	RHITexturePtr m_texture = nullptr;
	RHITextureROViewPtr m_texture_srv = nullptr;
	RHISamplerPtr m_texture_sampler = nullptr;
	RHISamplerPtr m_point_sampler = nullptr;

	//RHI
	// assets
	MeshAssetPtr m_demo_mesh = nullptr;
	std::string m_demo_mesh_asset_path = std::string( "#engine/Meshes/Cube.sea" );

	// descriptors
	std::vector<RHIDescriptorSetPtr> m_binding_tables;
	std::vector<RHIDescriptorSetPtr> m_rt_descsets;
	std::vector<RHIDescriptorSetPtr> m_fsquad_descsets;

	World m_world;
	World::Entity m_demo_mesh_entity = World::Entity::nullid;

	std::unique_ptr<Scene> m_scene;
	std::unique_ptr<SceneView> m_scene_view;

	// GUI state
	std::string m_new_entity_name;
	bool m_show_imgui_demo = false;
	bool m_show_world_outliner = false;
	float m_fov_degrees = 45.0f;

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

	void CreatePipeline();

	void BuildRendergraphRT( Rendergraph& rendergraph, RHICommandList* ui_cmd_list );

	void UpdateGui();

	void CleanupPipeline();

	void CreateVertexBuffer();

	void CreateIndexBuffer();

	void CopyBufferToImage( RHIBuffer& src, RHITexture& image, uint32_t width, uint32_t height );

	void CreateDescriptorSetLayout();

	void CreateUniformBuffers();

	void UpdateUniformBuffer( uint32_t current_image );

	void CreateDescriptorSets();

	void CreateTextureImage();

	void CreateImage(
		uint32_t width, uint32_t height, RHIFormat format, RHITextureUsageFlags usage, RHITextureLayout layout,
		RHITexturePtr& image );

	void TransitionImageLayoutAndFlush( RHITexture& texture, RHITextureLayout old_layout, RHITextureLayout new_layout );
	void TransitionImageLayout( RHICommandList& cmd_list, RHITexture& texture, RHITextureLayout old_layout, RHITextureLayout new_layout );

	void CreateTextureImageView();

	void CreateTextureSampler();

	void CreateCubePipeline();

	void CreateRTPipeline();

	void CreateFullscreenQuadPipeline();

	void UpdateScene();
};