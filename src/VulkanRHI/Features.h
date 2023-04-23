#pragma once

#include "StdAfx.h"

struct VulkanFeatures
{
    VkPhysicalDeviceVulkan13Features vk13 = {};
    VkPhysicalDeviceVulkan12Features vk12 = {};

    VkPhysicalDeviceAccelerationStructureFeaturesKHR as = {};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtpipe = {};

    VkPhysicalDeviceFeatures2 features2_head = {};

    void InitFromDevice( VkPhysicalDevice device, bool need_raytracing );
    void LinkPointers( bool need_rt );
    VulkanFeatures& operator=( const VulkanFeatures& );
    VulkanFeatures( const VulkanFeatures& );
    VulkanFeatures() = default;
};