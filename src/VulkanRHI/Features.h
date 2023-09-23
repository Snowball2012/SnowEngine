#pragma once

#include "StdAfx.h"

struct VulkanFeatures
{
    VkPhysicalDeviceVulkan13Features vk13 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    VkPhysicalDeviceVulkan12Features vk12 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };

    VkPhysicalDeviceAccelerationStructureFeaturesKHR as = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtpipe = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };

    VkPhysicalDeviceFeatures2 features2_head = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };

    VkPhysicalDeviceProperties2 props2_head = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_pipe_props = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
    VkPhysicalDeviceAccelerationStructurePropertiesKHR as_props = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR };

    VkPhysicalDevice16BitStorageFeatures vk_16bit_features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES };

    void InitFromDevice( VkPhysicalDevice device, bool need_raytracing );
    void LinkPointers( bool need_rt );
    VulkanFeatures& operator=( const VulkanFeatures& );
    VulkanFeatures( const VulkanFeatures& );
    VulkanFeatures() = default;
};