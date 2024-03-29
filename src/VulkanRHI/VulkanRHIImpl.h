#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

#include "VulkanRHI.h"

#include "Swapchain.h"

#include "Features.h"

class VulkanSemaphore : public RHISemaphore
{
	GENERATE_RHI_OBJECT_BODY()
private:
	VkSemaphore m_vk_semaphore = VK_NULL_HANDLE;

public:
	VulkanSemaphore( class VulkanRHI* rhi );

	virtual ~VulkanSemaphore() override;

	VkSemaphore GetVkSemaphore() const { return m_vk_semaphore; }
};

struct QueueFamilyIndices
{
	std::optional<uint32_t> graphics;
	std::optional<uint32_t> present;

	bool IsComplete() const { return graphics.has_value() && present.has_value(); }
};

struct VulkanQueue
{
	VkQueue vk_handle = VK_NULL_HANDLE;
	uint64_t submitted_counter = 0;
	uint64_t completed_counter = 0;
};

QueueFamilyIndices FindQueueFamilies( VkPhysicalDevice device, VkSurfaceKHR surface );

using VulkanSwapChainPtr = boost::intrusive_ptr<VulkanSwapChain>;

class VulkanRHI : public RHI
{
private:
	static constexpr uint32_t VulkanVersion = VK_API_VERSION_1_3;

	VkInstance m_vk_instance = VK_NULL_HANDLE;

	IVulkanWindowInterface* m_window_iface = nullptr;

	std::unique_ptr<class VulkanValidationCallback> m_validation_callback = nullptr;

	VkSurfaceKHR m_main_surface = VK_NULL_HANDLE;

	VkPhysicalDevice m_vk_phys_device = VK_NULL_HANDLE;

	VulkanFeatures m_vk_features = {};

	VkPhysicalDeviceProperties m_vk_phys_device_props = {};

	std::vector<const char*> m_required_device_extensions =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	std::vector<const char*> m_raytracing_extensions =
	{
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
	};

	VkDevice m_vk_device = VK_NULL_HANDLE;

	VmaAllocator m_vma = VK_NULL_HANDLE;

	QueueFamilyIndices m_queue_family_indices;

	VulkanQueue m_graphics_queue;
	VulkanQueue m_present_queue;

	VulkanSwapChainPtr m_main_swap_chain = nullptr;

	VkCommandPool m_cmd_pool = VK_NULL_HANDLE;

	VkDescriptorPool m_desc_pool = VK_NULL_HANDLE;

	std::unique_ptr<class VulkanCommandListManager> m_cmd_list_mgr;

	std::vector<RHIObject*> m_objects_to_delete;

	SpinLock m_loaded_shaders_lock;
	std::set<class Shader*> m_loaded_shaders;

	SpinLock m_loaded_psos_lock;
	std::set<class VulkanGraphicsPSO*> m_loaded_psos;
	std::set<class VulkanRaytracingPSO*> m_loaded_rt_psos;
	std::set<class VulkanComputePSO*> m_loaded_compute_psos;

	bool m_raytracing_supported = false;

public:
	VulkanRHI( const VulkanRHICreateInfo& info );
	virtual ~VulkanRHI() override;

	virtual void LoadExtensions();

	virtual bool SupportsRaytracing() const override { return m_raytracing_supported; }

	virtual RHISwapChain* GetMainSwapChain() override { return m_main_swap_chain.get(); }

	virtual RHISemaphore* CreateGPUSemaphore() override;

	virtual void Present( RHISwapChain& swap_chain, const PresentInfo& info ) override;

	virtual void WaitIdle() override;

	virtual RHICommandList* GetCommandList( QueueType type ) override;

	virtual RHIFence SubmitCommandLists( const SubmitInfo& info ) override;

	virtual void WaitForFenceCompletion( const RHIFence& fence ) override;

	virtual RHIShader* CreateShader( const ShaderCreateInfo& create_info ) override;

	virtual RHIDescriptorSetLayout* CreateDescriptorSetLayout( const DescriptorSetLayoutInfo& info ) override;
	virtual RHIShaderBindingLayout* CreateShaderBindingLayout( const ShaderBindingLayoutInfo& info ) override;

	virtual RHIGraphicsPipeline* CreatePSO( const RHIGraphicsPipelineInfo& pso_info ) override;
	virtual RHIRaytracingPipeline* CreatePSO( const RHIRaytracingPipelineInfo& pso_info ) override;
	virtual RHIComputePipeline* CreatePSO( const RHIComputePipelineInfo& pso_info ) override;

	virtual RHIBuffer* CreateDeviceBuffer( const BufferInfo& buf_info ) override;
	virtual RHIUploadBuffer* CreateUploadBuffer( const BufferInfo& buf_info ) override;
	virtual RHIReadbackBuffer* CreateReadbackBuffer( const BufferInfo& buf_info ) override;

	virtual RHIASInstanceBuffer* CreateASInstanceBuffer( const ASInstanceBufferInfo& info ) override;

	virtual RHITexture* CreateTexture( const TextureInfo& tex_info ) override;

	virtual RHISampler* CreateSampler( const SamplerInfo& info ) override;

	virtual RHIDescriptorSet* CreateDescriptorSet( RHIDescriptorSetLayout& layout ) override;

	virtual RHIAccelerationStructure* CreateAS( const RHI::ASInfo& info ) override;

	virtual RHIUniformBufferView* CreateUniformBufferView( const RHIBufferViewInfo& info ) override;
	virtual RHITextureROView* CreateTextureROView( const TextureROViewInfo& info ) override;
	virtual RHITextureRWView* CreateTextureRWView( const TextureRWViewInfo& info ) override;
	virtual RHIRenderTargetView* CreateRTV( const RenderTargetViewInfo& info ) override;

	virtual bool ReloadAllShaders() override;

	virtual bool GetASBuildSize( RHIAccelerationStructureType type, const RHIASGeometryInfo* geom_infos, size_t num_geoms, RHIASBuildSizes& out_sizes ) override;

	virtual uint64_t GetUniformBufferMinAlignment() const override { return m_vk_phys_device_props.limits.minUniformBufferOffsetAlignment; }

	VkPhysicalDevice GetPhysDevice() const { return m_vk_phys_device; }
	VkDevice GetDevice() const { return m_vk_device; }
	const QueueFamilyIndices& GetQueueFamilyIndices() const { return m_queue_family_indices; }
	IVulkanWindowInterface* GetWindowIface() const { return m_window_iface; }

	VkImageView CreateImageView( VkImage image, VkFormat format );
	void TransitionImageLayoutAndFlush( VkImage image, VkImageLayout old_layout, VkImageLayout new_layout );

	VulkanQueue* GetQueue( QueueType type );

	VmaAllocator GetVMA() const { return m_vma; }

	const VkPhysicalDeviceProperties& GetPhysDeviceProps() const { return m_vk_phys_device_props; }

	VkDescriptorPool GetVkDescriptorPool() const { return m_desc_pool; }

	// Should probably be thread-safe
	// This method places an image layout transition call before any command list is executed on the next submit
	// Should probably be used only to transition textures to their initial layout on creation
	void DeferImageLayoutTransition( VkImage image, RHI::QueueType queue_type, VkImageLayout old_layout, VkImageLayout new_layout );

	// Has to be thread-safe
	void DeferredDestroyRHIObject( RHIObject* obj );

	static VkPipelineStageFlags GetVkPipelineStageFlags( RHIPipelineStageFlags rhi_flags );
	static VkShaderStageFlags GetVkShaderStageFlags( RHIShaderStageFlags rhi_flags );

	static VkFormat GetVkFormat( RHIFormat format );
	static RHIFormat GetRHIFormat( VkFormat format );
	static uint32_t GetVkFormatSize( RHIFormat format );

	static VkVertexInputRate GetVkVertexInputRate( RHIPrimitiveFrequency frequency );

	static VkBufferUsageFlags GetVkBufferUsageFlags( RHIBufferUsageFlags usage );
	static VkImageUsageFlags GetVkImageUsageFlags( RHITextureUsageFlags usage );

	static VkImageType GetVkImageType( RHITextureDimensions dimensions );

	static VkDescriptorType GetVkDescriptorType( RHIShaderBindingType type );

	static VkImageLayout GetVkImageLayout( RHITextureLayout layout );

	static void GetStagesAndAccessMasksForLayoutBarrier(
		VkImageLayout src_layout, VkImageLayout dst_layout,
		VkPipelineStageFlags& src_stage, VkPipelineStageFlags& dst_stage,
		VkImageMemoryBarrier& barrier );

	static VkAttachmentLoadOp GetVkLoadOp( RHILoadOp op );
	static VkAttachmentStoreOp GetVkStoreOp( RHIStoreOp op );

	static VkCullModeFlags GetCullModeFlags( RHICullModeFlags rhi_flags );

	static VkBlendFactor GetVkBlendFactor( RHIBlendFactor rhi_blend_factor );

	static VkAccelerationStructureTypeKHR GetVkASType( RHIAccelerationStructureType type );

	static VkIndexType GetVkIndexType( RHIIndexBufferType type );
	static uint8_t GetVkIndexTypeByteSize( RHIIndexBufferType type );

	// if instance_data is not null, convert geom_info transform array to vk transform array
	static bool GetVkASGeometry(
		const RHIASGeometryInfo& geom_info, VkAccelerationStructureGeometryKHR& vk_geom_info, uint32_t& primitive_count );

	void TransitionImageLayout( VkCommandBuffer buf, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout );

	void RegisterLoadedShader( Shader& shader );
	void UnregisterLoadedShader( Shader& shader );

	void RegisterLoadedPSO( VulkanGraphicsPSO& pso );
	void UnregisterLoadedPSO( VulkanGraphicsPSO& pso );

	void RegisterLoadedPSO( VulkanRaytracingPSO& pso );
	void UnregisterLoadedPSO( VulkanRaytracingPSO& pso );

	void RegisterLoadedPSO( VulkanComputePSO& pso );
	void UnregisterLoadedPSO( VulkanComputePSO& pso );

	bool ReloadAllPipelines();

	const VulkanFeatures& GetFeatures() const { return m_vk_features; }

private:
	void CreateVkInstance( const VulkanRHICreateInfo& info );

	void LogSupportedVkInstanceExtensions() const;

	std::vector<const char*> GetRequiredExtensions( bool enable_validation_layers ) const;
	std::vector<const char*> GetSupportedLayers( const std::vector<const char*> wanted_layers ) const;

	void PickPhysicalDevice( VkSurfaceKHR surface, bool need_raytracing );
	bool IsDeviceSuitable( VkPhysicalDevice device, VkSurfaceKHR surface, bool need_raytracing, const VulkanFeatures& features ) const;

	struct ExtensionCheckResult
	{
		bool all_required_extensions_found = false;
		bool raytracing_extensions_found = false;
	};
	ExtensionCheckResult CheckDeviceExtensionSupport( VkPhysicalDevice device ) const;

	void CreateLogicalDevice();

	void CreateVMA();

	VulkanSwapChain* CreateSwapChainInternal( const VulkanSwapChainCreateInfo& create_info );

	void CreateCommandPool();

	void CreateDescriptorPool();

	VkCommandBuffer BeginSingleTimeCommands();
	void EndSingleTimeCommands( VkCommandBuffer buf );
};
