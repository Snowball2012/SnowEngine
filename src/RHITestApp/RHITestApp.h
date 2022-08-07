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

	static VkVertexInputBindingDescription GetBindingDescription();
	static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions();
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
	VkPhysicalDevice m_vk_phys_device = VK_NULL_HANDLE;
	VkDevice m_vk_device = VK_NULL_HANDLE;
	VkQueue m_graphics_queue = VK_NULL_HANDLE;

	// surface
	RHIObjectPtr<RHISwapChain> m_swapchain = nullptr;

	// pipeline
	VkDescriptorSetLayout m_descriptor_layout = VK_NULL_HANDLE;
	VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
	VkPipeline m_graphics_pipeline = VK_NULL_HANDLE;

	// commands
	VkCommandPool m_cmd_pool = VK_NULL_HANDLE;

	// synchronization
	std::vector<RHIObjectPtr<RHISemaphore>> m_image_available_semaphores;
	std::vector<RHIObjectPtr<RHISemaphore>> m_render_finished_semaphores;
	std::vector<RHIFence> m_inflight_fences;
	uint32_t m_current_frame = 0;

	// resources
	VkBuffer m_vertex_buffer = VK_NULL_HANDLE;
	VkDeviceMemory m_vb_memory = VK_NULL_HANDLE;
	VkBuffer m_index_buffer = VK_NULL_HANDLE;
	VkDeviceMemory m_ib_memory = VK_NULL_HANDLE;
	std::vector<VkBuffer> m_uniform_buffers;
	std::vector<VkDeviceMemory> m_ub_memory;
	// images
	VkImage m_texture_image = VK_NULL_HANDLE;
	VkDeviceMemory m_texture_memory = VK_NULL_HANDLE;
	VkImageView m_texture_view = VK_NULL_HANDLE;
	VkSampler m_texture_sampler = VK_NULL_HANDLE;

	// descriptors
	VkDescriptorPool m_desc_pool = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> m_desc_sets;

	bool m_fb_resized = false;

	std::vector<const char*> m_required_device_extensions =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

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

	void RecordCommandBuffer(VkCommandBuffer buf, VkImage swapchain_image, VkImageView swapchain_image_view);

	void DrawFrame();

	void CreateSyncObjects();

	void RecreateSwapChain();

	void CleanupPipeline();

	void CreateVertexBuffer();

	void CreateIndexBuffer();

	void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& buffer_mem);

	void CopyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);
	void CopyBufferToImage(VkBuffer src, VkImage image, uint32_t width, uint32_t height);

	uint32_t FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties);

	void CreateDescriptorSetLayout();

	void CreateUniformBuffers();

	void UpdateUniformBuffer(uint32_t current_image);

	void CreateDescriptorPool();

	void CreateDescriptorSets();

	void CreateTextureImage();

	void CreateImage(
		uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
		VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& image_mem);

	VkCommandBuffer BeginSingleTimeCommands();
	void EndSingleTimeCommands(VkCommandBuffer buf);

	void TransitionImageLayoutAndFlush(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout);
	void TransitionImageLayout(VkCommandBuffer buf, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout);

	void CreateTextureImageView();

	VkImageView CreateImageView(VkImage image, VkFormat format);

	void CreateTextureSampler();

};