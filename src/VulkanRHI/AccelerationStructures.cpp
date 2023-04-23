#include "StdAfx.h"

#include "AccelerationStructures.h"

#include "VulkanRHIImpl.h"

IMPLEMENT_RHI_OBJECT( VulkanAccelerationStructure )

VulkanAccelerationStructure::VulkanAccelerationStructure( VulkanRHI* rhi, const RHI::ASInfo& info )
    : m_rhi( rhi )
{
    RHI::BufferInfo buf_info = {};
    buf_info.size = info.size;
    buf_info.usage = RHIBufferUsageFlags::AccelerationStructure;
    m_underlying_buffer = RHIImpl( m_rhi->CreateDeviceBuffer( buf_info ) );

    m_vk_type = VulkanRHI::GetVkASType( info.type );

    VkAccelerationStructureCreateInfoKHR vk_ci = {};
    vk_ci.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    vk_ci.type = m_vk_type;
    vk_ci.buffer = m_underlying_buffer->GetVkBuffer();
    VK_VERIFY( vkCreateAccelerationStructureKHR( m_rhi->GetDevice(), &vk_ci, nullptr, &m_vk_as ) );
}

VulkanAccelerationStructure::~VulkanAccelerationStructure()
{
    vkDestroyAccelerationStructureKHR( m_rhi->GetDevice(), m_vk_as, nullptr);
}
