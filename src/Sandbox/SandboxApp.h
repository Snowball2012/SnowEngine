#pragma once

#include "StdAfx.h"

#include <ecs/EntityContainer.h>

#include <Engine/EngineApp.h>
#include <Engine/Assets.h>
#include <Engine/RHIUtils.h>

#include "RenderResources.h"

SE_LOG_CATEGORY( Sandbox );

struct NameComponent
{
	std::string name;
};

using World = EntityContainer<
	NameComponent
>;

class SandboxApp : public EngineApp
{
private:
	// pipeline
	RHIDescriptorSetLayoutPtr m_binding_table_layout = nullptr;
	RHIShaderBindingLayoutPtr m_shader_bindings_layout = nullptr;
	RHIGraphicsPipelinePtr m_rhi_graphics_pipeline = nullptr;
	RHIGraphicsPipelinePtr m_cube_graphics_pipeline = nullptr;

	// resources
	RHIBufferPtr m_vertex_buffer = nullptr;
	RHIBufferPtr m_index_buffer = nullptr;
	std::vector<RHIUploadBufferPtr> m_uniform_buffers;
	std::vector<RHICBVPtr> m_uniform_buffer_views;
	// images
	RHITexturePtr m_texture = nullptr;
	RHITextureSRVPtr m_texture_srv = nullptr;
	RHISamplerPtr m_texture_sampler = nullptr;
	// assets
	CubeAssetPtr m_cube = nullptr;


	// descriptors
	std::vector<RHIDescriptorSetPtr> m_binding_tables;

	World m_world;
	TLAS m_tlas;
	TLAS::InstanceID m_cube_instance_tlas_id = TLAS::InstanceID::nullid;

	// GUI state
	std::string m_new_entity_name;
	bool m_show_imgui_demo = false;
	bool m_show_world_outliner = false;
	bool m_show_cube = false;

public:
	SandboxApp();
	~SandboxApp();

private:
	virtual void OnInit() override;

	virtual void OnCleanup() override;

	virtual void OnDrawFrame( std::vector<RHICommandList*>& lists_to_submit ) override;

	virtual void OnUpdate() override;

	virtual const char* GetMainWindowName() const override { return "SnowEngine Sandbox"; }
	virtual const char* GetAppName() const override { return "SnowEngineSandbox"; }

	void CreatePipeline();

	void RecordCommandBuffer( RHICommandList& list, RHISwapChain& swapchain );

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
};