#include "StdAfx.h"

#include "Features.h"

void VulkanFeatures::InitFromDevice( VkPhysicalDevice device, bool need_raytracing )
{
    LinkPointers( need_raytracing );

    vkGetPhysicalDeviceFeatures2( device, &features2_head );

    vkGetPhysicalDeviceProperties2( device, &props2_head );
}

void VulkanFeatures::LinkPointers( bool need_rt )
{
    // features
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

    // properties
    {
        void* current_props_head = nullptr;

        if ( need_rt ) {
            rt_pipe_props.pNext = current_props_head;
            current_props_head = &rt_pipe_props;

            as_props.pNext = current_props_head;
            current_props_head = &as_props;
        }

        props2_head.pNext = current_props_head;
    }
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
