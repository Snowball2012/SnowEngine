#include "StdAfx.h"

#include "Extensions.h"

#define DEFINE_VK_FUNCTION1( rettype, name, t0, p0 ) \
    PFN_##name pfn_##name = nullptr; \
    rettype name ( t0 p0 ) { return pfn_##name( p0 ); }

#define DEFINE_VK_FUNCTION2( rettype, name, t0, p0, t1, p1 ) \
    PFN_##name pfn_##name = nullptr; \
    rettype name ( t0 p0, t1 p1 ) { return pfn_##name( p0, p1 ); }

#define DEFINE_VK_FUNCTION3( rettype, name, t0, p0, t1, p1, t2, p2 ) \
    PFN_##name pfn_##name = nullptr; \
    rettype name ( t0 p0, t1 p1, t2 p2 ) { return pfn_##name( p0, p1, p2 ); }

#define DEFINE_VK_FUNCTION4( rettype, name, t0, p0, t1, p1, t2, p2, t3, p3 ) \
    PFN_##name pfn_##name = nullptr; \
    rettype name ( t0 p0, t1 p1, t2 p2, t3 p3 ) { return pfn_##name( p0, p1, p2, p3 ); }

#define DEFINE_VK_FUNCTION5( rettype, name, t0, p0, t1, p1, t2, p2, t3, p3, t4, p4 ) \
    PFN_##name pfn_##name = nullptr; \
    rettype name ( t0 p0, t1 p1, t2 p2, t3 p3, t4 p4 ) { return pfn_##name( p0, p1, p2, p3, p4 ); }

// raytracing

DEFINE_VK_FUNCTION4( VkResult, vkCreateAccelerationStructureKHR,
    VkDevice, device,
    const VkAccelerationStructureCreateInfoKHR*, pCreateInfo,
    const VkAllocationCallbacks*, pAllocator,
    VkAccelerationStructureKHR*, pAccelerationStructure )

DEFINE_VK_FUNCTION3( void, vkDestroyAccelerationStructureKHR,
    VkDevice,                                    device,
    VkAccelerationStructureKHR,                  accelerationStructure,
    const VkAllocationCallbacks*, pAllocator );

DEFINE_VK_FUNCTION4( void, vkCmdBuildAccelerationStructuresKHR,
    VkCommandBuffer,                             commandBuffer,
    uint32_t,                                    infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR*, pInfos,
    const VkAccelerationStructureBuildRangeInfoKHR* const*, ppBuildRangeInfos );

DEFINE_VK_FUNCTION5( void, vkGetAccelerationStructureBuildSizesKHR,
    VkDevice,                                    device,
    VkAccelerationStructureBuildTypeKHR,         buildType,
    const VkAccelerationStructureBuildGeometryInfoKHR*, pBuildInfo,
    const uint32_t*, pMaxPrimitiveCounts,
    VkAccelerationStructureBuildSizesInfoKHR*, pSizeInfo );

void Extensions::LoadRT( VkDevice device )
{
#ifdef DEVICE_LEVEL_VULKAN_FUNCTION
#error "DEVICE_LEVEL_VULKAN_FUNCTION is already defined"
#else
#define DEVICE_LEVEL_VULKAN_FUNCTION( name )                                  \
pfn_##name = ( decltype( pfn_##name ) )vkGetDeviceProcAddr( device, #name );  \
VERIFY_NOT_EQUAL( name, nullptr );
#endif

    DEVICE_LEVEL_VULKAN_FUNCTION( vkCreateAccelerationStructureKHR );
    DEVICE_LEVEL_VULKAN_FUNCTION( vkDestroyAccelerationStructureKHR );
    DEVICE_LEVEL_VULKAN_FUNCTION( vkCmdBuildAccelerationStructuresKHR );
    DEVICE_LEVEL_VULKAN_FUNCTION( vkGetAccelerationStructureBuildSizesKHR );

#undef DEVICE_LEVEL_VULKAN_FUNCTION
}
