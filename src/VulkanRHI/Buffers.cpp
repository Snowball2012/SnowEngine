#include "StdAfx.h" 

#include "Buffers.h"

#include "VulkanRHIImpl.h"

IMPLEMENT_RHI_OBJECT(VulkanBuffer)

VulkanBuffer::VulkanBuffer(VulkanRHI* rhi, const RHI::BufferInfo& info, VmaMemoryUsage usage, VmaAllocationCreateFlags alloc_create_flags)
	: m_rhi(rhi)
{
    VkBufferCreateInfo vk_create_info = {};
    vk_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vk_create_info.size = uint32_t(info.size);
    vk_create_info.usage = VulkanRHI::GetVkBufferUsageFlags(info.usage);
    vk_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vma_alloc_info = {};
    vma_alloc_info.usage = usage;
    vma_alloc_info.flags = alloc_create_flags;
    VK_VERIFY(vmaCreateBuffer(m_rhi->GetVMA(), &vk_create_info, &vma_alloc_info, &m_vk_buffer, &m_allocation, nullptr));
}

void VulkanBuffer::GetAllocationInfo(VmaAllocationInfo& info)
{
    vmaGetAllocationInfo(m_rhi->GetVMA(), m_allocation, &info);
}

VulkanBuffer::~VulkanBuffer()
{
    vmaDestroyBuffer(m_rhi->GetVMA(), m_vk_buffer, m_allocation);
}

IMPLEMENT_RHI_OBJECT(VulkanUploadBuffer)

VulkanUploadBuffer::VulkanUploadBuffer(VulkanRHI* rhi, const RHI::BufferInfo& info)
    : m_rhi(rhi)
{
    m_buffer = new VulkanBuffer(
        rhi, info, VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
}

VulkanUploadBuffer::~VulkanUploadBuffer()
{
}

void VulkanUploadBuffer::WriteBytes(const void* src, size_t size, size_t offset)
{
    VmaAllocationInfo info;
    m_buffer->GetAllocationInfo(info);
    VERIFY_NOT_EQUAL(info.pMappedData, nullptr);
    memcpy((uint8_t*)info.pMappedData + offset, src, size);
}
