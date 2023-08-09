#include "StdAfx.h" 

#include "Buffers.h"

#include "VulkanRHIImpl.h"

IMPLEMENT_RHI_OBJECT( VulkanBuffer )

VulkanBuffer::VulkanBuffer( VulkanRHI* rhi, const RHI::BufferInfo& info, VmaMemoryUsage usage, VmaAllocationCreateFlags alloc_create_flags, VkMemoryPropertyFlags alloc_required_flags )
    : m_rhi( rhi )
{
    VkBufferCreateInfo vk_create_info = {};
    vk_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vk_create_info.size = uint32_t( info.size );
    vk_create_info.usage = VulkanRHI::GetVkBufferUsageFlags( info.usage ) | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT; // adding device address here makes everything easier. Not sure if it has any performance implications
    vk_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vma_alloc_info = {};
    vma_alloc_info.usage = usage;
    vma_alloc_info.flags = alloc_create_flags;
    vma_alloc_info.requiredFlags = alloc_required_flags;
    VK_VERIFY( vmaCreateBuffer( m_rhi->GetVMA(), &vk_create_info, &vma_alloc_info, &m_vk_buffer, &m_allocation, nullptr ) );

    VkBufferDeviceAddressInfo da_info = {};
    da_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    da_info.buffer = m_vk_buffer;
    m_device_address = vkGetBufferDeviceAddress( m_rhi->GetDevice(), &da_info );

    // VmaAllocationInfo::size may be larger than requested, but it can only be used for host memory operations and not for device operations with the resource. Use buffer size instead
    m_buffer_size = info.size;
}

void VulkanBuffer::GetAllocationInfo( VmaAllocationInfo& info ) const
{
    vmaGetAllocationInfo( m_rhi->GetVMA(), m_allocation, &info );
}

VulkanBuffer::~VulkanBuffer()
{
    vmaDestroyBuffer( m_rhi->GetVMA(), m_vk_buffer, m_allocation );
}

size_t VulkanBuffer::GetSize() const
{
    return m_buffer_size;
}

IMPLEMENT_RHI_OBJECT( VulkanUploadBuffer )

VulkanUploadBuffer::VulkanUploadBuffer( VulkanRHI* rhi, const RHI::BufferInfo& info )
    : m_rhi( rhi )
{
    m_buffer = new VulkanBuffer(
        rhi, info, VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ); // we want to avoid having to vkFlushMappedMemoryRanges every time we write to a buffer
}

VulkanUploadBuffer::~VulkanUploadBuffer()
{
}

void VulkanUploadBuffer::WriteBytes( const void* src, size_t size, size_t offset )
{
    VmaAllocationInfo info;
    m_buffer->GetAllocationInfo( info );
    VERIFY_NOT_EQUAL( info.pMappedData, nullptr );
    memcpy( ( uint8_t* )info.pMappedData + offset, src, size );
}
