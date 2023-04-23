#include "StdAfx.h"

#include "Features.h"

void VulkanFeatures::InitFromDevice( VkPhysicalDevice device, bool need_raytracing )
{
    vk13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

    vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

    as.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

    rtpipe.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

    features2_head.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    LinkPointers( need_raytracing );

    vkGetPhysicalDeviceFeatures2( device, &features2_head );
}

void VulkanFeatures::LinkPointers( bool need_rt )
{
    vk13.pNext = nullptr;
    void* current_features_head = &vk13;
    vk12.pNext = current_features_head;
    current_features_head = &vk12;

    if ( need_rt ) {
        as.pNext = current_features_head;
        rtpipe.pNext = &as;
        current_features_head = &rtpipe;
    }

    features2_head.pNext = current_features_head;
}

VulkanFeatures& VulkanFeatures::operator=( const VulkanFeatures& rhs )
{
    memcpy( this, &rhs, sizeof( VulkanFeatures ) );
    
    // a weird way to determine if rt is needed. But should work ok
    LinkPointers( as.pNext != nullptr );

    return *this;
}

VulkanFeatures::VulkanFeatures( const VulkanFeatures& rhs )
{
    *this = rhs;
}
