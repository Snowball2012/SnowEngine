#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

#include "VulkanRHI.h"

#include "Swapchain.h"

class VulkanSemaphore : public RHISemaphore
{
private:
	VulkanRHI* m_rhi = nullptr;
	VkSemaphore m_vk_semaphore = VK_NULL_HANDLE;

public:
	VulkanSemaphore(class VulkanRHI* rhi);

	virtual ~VulkanSemaphore() override;

	virtual void* GetNativeSemaphore() const override { return (void*)&m_vk_semaphore; }

	VkSemaphore GetVkSemaphore() const { return m_vk_semaphore; }

	virtual void AddRef() override;
	virtual void Release() override;
};

struct QueueFamilyIndices
{
	std::optional<uint32_t> graphics;
	std::optional<uint32_t> present;

	bool IsComplete() const { return graphics.has_value() && present.has_value(); }
};

QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

using VulkanSwapChainPtr = boost::intrusive_ptr<VulkanSwapChain>;

class VulkanRHI : public RHI
{
private:
	VkInstance m_vk_instance = VK_NULL_HANDLE;

	IVulkanWindowInterface* m_window_iface = nullptr;

	std::unique_ptr<class VulkanValidationCallback> m_validation_callback = nullptr;

	VkSurfaceKHR m_main_surface = VK_NULL_HANDLE;

	VkPhysicalDevice m_vk_phys_device = VK_NULL_HANDLE;

	std::vector<const char*> m_required_device_extensions =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	VkDevice m_vk_device = VK_NULL_HANDLE;

	QueueFamilyIndices m_queue_family_indices;

	VkQueue m_graphics_queue = VK_NULL_HANDLE;
	VkQueue m_present_queue = VK_NULL_HANDLE;

	VulkanSwapChainPtr m_main_swap_chain = nullptr;

	VkCommandPool m_cmd_pool = VK_NULL_HANDLE;

	std::unique_ptr<class VulkanCommandListManager> m_cmd_list_mgr;

public:
	VulkanRHI(const VulkanRHICreateInfo& info);
	virtual ~VulkanRHI() override;

	virtual RHISwapChain* GetMainSwapChain() override { return m_main_swap_chain.get(); }

	virtual RHISemaphore* CreateGPUSemaphore() override;

	virtual void Present(RHISwapChain& swap_chain, const PresentInfo& info) override;

	virtual void WaitIdle() override;

	virtual RHICommandList* GetCommandList(QueueType type) override;

	virtual void SubmitCommandLists(const SubmitInfo& info) override;

	// TEMP
	virtual void* GetNativePhysDevice() const override { return (void*)&m_vk_phys_device; }
	virtual void* GetNativeSurface() const override { return (void*)&m_main_surface; }
	virtual void* GetNativeDevice() const override { return (void*)&m_vk_device; }
	virtual void* GetNativeGraphicsQueue() const override { return (void*)&m_graphics_queue; }
	virtual void* GetNativeCommandPool() const override { return (void*)&m_cmd_pool; }

	VkPhysicalDevice GetPhysDevice() const { return m_vk_phys_device; }
	VkDevice GetDevice() const { return m_vk_device; }
	const QueueFamilyIndices& GetQueueFamilyIndices() const { return m_queue_family_indices; }
	IVulkanWindowInterface* GetWindowIface() const { return m_window_iface; }

	VkImageView CreateImageView(VkImage image, VkFormat format);
	void TransitionImageLayoutAndFlush(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout);

	VkQueue GetQueue(QueueType type) const;

	static VkPipelineStageFlagBits GetVkStageFlags(PipelineStageFlags rhi_flags);

private:
	void CreateVkInstance(const VulkanRHICreateInfo& info);

	void LogSupportedVkInstanceExtensions() const;

	std::vector<const char*> GetRequiredExtensions(bool enable_validation_layers) const;
	std::vector<const char*> GetSupportedLayers(const std::vector<const char*> wanted_layers) const;

	void PickPhysicalDevice(VkSurfaceKHR surface);
	bool IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) const;

	bool CheckDeviceExtensionSupport(VkPhysicalDevice device) const;

	void CreateLogicalDevice();

	VulkanSwapChain* CreateSwapChainInternal(const VulkanSwapChainCreateInfo& create_info);

	void CreateCommandPool();

	VkCommandBuffer BeginSingleTimeCommands();
	void EndSingleTimeCommands(VkCommandBuffer buf);

	void TransitionImageLayout(VkCommandBuffer buf, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout);

};
