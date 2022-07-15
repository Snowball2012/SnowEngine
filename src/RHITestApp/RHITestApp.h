#pragma once

#include "StdAfx.h"

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

	// instance
	VkInstance m_vk_instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT m_vk_dbg_messenger = VK_NULL_HANDLE;
	bool m_enable_validation_layer = false;

	// device
	VkPhysicalDevice m_vk_phys_device = VK_NULL_HANDLE;
	VkDevice m_vk_device = VK_NULL_HANDLE;
	VkQueue m_graphics_queue = VK_NULL_HANDLE;
	VkQueue m_present_queue = VK_NULL_HANDLE;

	// surface
	VkSurfaceKHR m_surface = VK_NULL_HANDLE;
	VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
	std::vector<VkImage> m_swapchain_images;
	std::vector<VkImageView> m_swapchain_image_views;
	std::vector<VkFramebuffer> m_swapchain_framebuffers;
	VkExtent2D m_swapchain_size_pixels = { 0, 0 };
	VkSurfaceFormatKHR m_swapchain_format = {};

	// pipeline
	VkDescriptorSetLayout m_descriptor_layout = VK_NULL_HANDLE;
	VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
	VkRenderPass m_render_pass = VK_NULL_HANDLE;
	VkPipeline m_graphics_pipeline = VK_NULL_HANDLE;

	// commands
	VkCommandPool m_cmd_pool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> m_cmd_buffers;

	// synchronization
	std::vector<VkSemaphore> m_image_available_semaphores;
	std::vector<VkSemaphore> m_render_finished_semaphores;
	std::vector<VkFence> m_inflight_fences;
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

public:
	RHITestApp();

	void Run();

private:
	void InitVulkan();
	void MainLoop();
	void Cleanup();

	void CreateVkInstance();
	void SetupDebugMessenger();
	void DestroyDebugMessenger();
	std::vector<const char*> GetSDLExtensions() const;
	std::vector<const char*> GetRequiredExtensions(bool enable_validation_layers) const;

	std::vector<const char*> GetSupportedLayers(const std::vector<const char*> wanted_layers) const;

	void LogSupportedVkInstanceExtensions() const;

	void PickPhysicalDevice();
	bool IsDeviceSuitable(VkPhysicalDevice device) const;

	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device) const;

	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) const;
	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& present_modes) const;
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;

	void CreateSwapChain();

	bool CheckDeviceExtensionSupport(VkPhysicalDevice device) const;

	void CreateLogicalDevice();

	void CreateSurface();

	void CreatePipeline();

	void CreateRenderPass();

	void CreateFramebuffers();

	void CreateCommandPool();

	void CreateCommandBuffer();

	void RecordCommandBuffer(VkCommandBuffer buf, uint32_t image_index);

	void DrawFrame();

	void CreateSyncObjects();

	void RecreateSwapChain();

	void CleanupSwapChain();

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

	void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout);

	void CreateTextureImageView();

	VkImageView CreateImageView(VkImage image, VkFormat format);

	void CreateTextureSampler();

};