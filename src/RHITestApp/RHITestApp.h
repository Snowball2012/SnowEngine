#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities = {};
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> present_modes;
};

struct Vertex
{
	glm::vec2 pos;
	alignas(16) glm::vec3 color;
	glm::vec2 texcoord;

	static std::array<RHIPrimitiveAttributeInfo, 3> GetRHIAttributes();
};

struct Matrices
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

class RHITestApp
{
private:
	static const size_t m_max_frames_in_flight = 2;

	glm::ivec2 m_window_size = glm::ivec2(800, 600);

	struct SDL_Window* m_main_wnd = nullptr;

	std::unique_ptr<struct SDLVulkanWindowInterface> m_window_iface = nullptr;

	// device
	VkDevice m_vk_device = VK_NULL_HANDLE;

	// surface
	RHIObjectPtr<RHISwapChain> m_swapchain = nullptr;

	// pipeline
	RHIShaderBindingTableLayoutPtr m_binding_table_layout = nullptr;
	RHIShaderBindingLayoutPtr m_shader_bindings_layout = nullptr;
	RHIGraphicsPipelinePtr m_rhi_graphics_pipeline = nullptr;

	// synchronization
	std::vector<RHIObjectPtr<RHISemaphore>> m_image_available_semaphores;
	std::vector<RHIObjectPtr<RHISemaphore>> m_render_finished_semaphores;
	std::vector<RHIFence> m_inflight_fences;
	uint32_t m_current_frame = 0;

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

	bool m_fb_resized = false;

	RHIPtr m_rhi;

public:
	RHITestApp();
	~RHITestApp();

	void Run();

private:
	void InitRHI();
	void MainLoop();
	void Cleanup();

	std::vector<const char*> GetSDLExtensions() const;

	void CreatePipeline();

	void RecordCommandBuffer(RHICommandList& list, RHISwapChain& swapchain);

	void DrawFrame();

	void CreateSyncObjects();

	void CleanupPipeline();

	void CreateVertexBuffer();

	void CreateIndexBuffer();

	void CopyBuffer(RHIBuffer& src, RHIBuffer& dst, size_t size);
	void CopyBufferToImage(VkBuffer src, VkImage image, uint32_t width, uint32_t height);

	void CreateDescriptorSetLayout();

	void CreateUniformBuffers();

	void UpdateUniformBuffer(uint32_t current_image);

	void CreateDescriptorSets();

	void CreateTextureImage();

	void CreateImage(
		uint32_t width, uint32_t height, RHIFormat format, RHITextureUsageFlags usage, RHITextureLayout layout,
		RHITexturePtr& image);

	RHICommandList* BeginSingleTimeCommands();
	void EndSingleTimeCommands(RHICommandList& buf);

	void TransitionImageLayoutAndFlush(RHITexture& texture, RHITextureLayout old_layout, RHITextureLayout new_layout);
	void TransitionImageLayout(RHICommandList& cmd_list, RHITexture& texture, RHITextureLayout old_layout, RHITextureLayout new_layout);

	void CreateTextureImageView();

	void CreateTextureSampler();

};