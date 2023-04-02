#pragma once

#include "StdAfx.h"

#include <ecs/EntityContainer.h>

#include <Engine/EngineApp.h>

#include "RenderResources.h"

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
	RHIShaderBindingTableLayoutPtr m_binding_table_layout = nullptr;
	RHIShaderBindingLayoutPtr m_shader_bindings_layout = nullptr;
	RHIGraphicsPipelinePtr m_rhi_graphics_pipeline = nullptr;

	// resources
	RHIBufferPtr m_vertex_buffer = nullptr;
	RHIBufferPtr m_index_buffer = nullptr;
	std::vector<RHIUploadBufferPtr> m_uniform_buffers;
	std::vector<RHICBVPtr> m_uniform_buffer_views;
	// images
	RHITexturePtr m_texture = nullptr;
	RHITextureSRVPtr m_texture_srv = nullptr;
	RHISamplerPtr m_texture_sampler = nullptr;

	// descriptors
	std::vector<RHIShaderBindingTablePtr> m_binding_tables;

	World m_world;

	// GUI state
	std::string m_new_entity_name;
	bool m_show_imgui_demo = false;
	bool m_show_world_outliner = false;

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

	void CopyBuffer( RHIBuffer& src, RHIBuffer& dst, size_t size );
	void CopyBufferToImage( RHIBuffer& src, RHITexture& image, uint32_t width, uint32_t height );

	void CreateDescriptorSetLayout();

	void CreateUniformBuffers();

	void UpdateUniformBuffer( uint32_t current_image );

	void CreateDescriptorSets();

	void CreateTextureImage();

	void CreateImage(
		uint32_t width, uint32_t height, RHIFormat format, RHITextureUsageFlags usage, RHITextureLayout layout,
		RHITexturePtr& image );

	RHICommandList* BeginSingleTimeCommands();
	void EndSingleTimeCommands( RHICommandList& buf );

	void TransitionImageLayoutAndFlush( RHITexture& texture, RHITextureLayout old_layout, RHITextureLayout new_layout );
	void TransitionImageLayout( RHICommandList& cmd_list, RHITexture& texture, RHITextureLayout old_layout, RHITextureLayout new_layout );

	void CreateTextureImageView();

	void CreateTextureSampler();
};