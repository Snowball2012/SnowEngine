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

	RHITexturePtr m_rt_frame = nullptr;
	RHITextureRWViewPtr m_frame_rwview = nullptr;
	RHITextureROViewPtr m_frame_roview = nullptr;
	//RHI
	// assets
	CubeAssetPtr m_cube = nullptr;


	// descriptors
	std::vector<RHIDescriptorSetPtr> m_binding_tables;
	std::vector<RHIDescriptorSetPtr> m_rt_descsets;
	std::vector<RHIDescriptorSetPtr> m_fsquad_descsets;

	World m_world;
	TLAS m_tlas;
	TLAS::InstanceID m_cube_instance_tlas_id = TLAS::InstanceID::nullid;

	// GUI state
	std::string m_new_entity_name;
	bool m_show_imgui_demo = false;
	bool m_show_world_outliner = false;
	bool m_show_cube = false;
	bool m_rt_path = false;
	bool m_use_rendergraph = false;

public:
	SandboxApp();
	~SandboxApp();

private:
	virtual void OnInit() override;

	virtual void OnCleanup() override;

	virtual void OnDrawFrame( std::vector<RHICommandList*>& lists_to_submit, Rendergraph& framegraph ) override;

	virtual void OnUpdate() override;

	virtual void OnSwapChainRecreated() override;

	virtual const char* GetMainWindowName() const override { return "SnowEngine Sandbox"; }
	virtual const char* GetAppName() const override { return "SnowEngineSandbox"; }

	void CreatePipeline();

	void RecordCommandBuffer( RHICommandList& list, RHISwapChain& swapchain );
	void RecordCommandBufferRT( RHICommandList& list, RHISwapChain& swapchain );

	void BuildRendergraphRT( Rendergraph& rendergraph );

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

	void CreateIntermediateBuffers();
};