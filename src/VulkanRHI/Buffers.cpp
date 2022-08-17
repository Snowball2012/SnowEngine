#include "StdAfx.h" 

#include "Buffers.h"

#include "VulkanRHIImpl.h"



VulkanUploadBuffer::VulkanUploadBuffer(VulkanRHI* rhi, const RHI::UploadBufferInfo& info)
	: m_rhi(rhi)
{
    VkBufferCreateInfo vk_create_info = {};
    vk_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vk_create_info.size = uint32_t(info.size);
    vk_create_info.usage = VulkanRHI::GetVkBufferUsageFlags(info.usage);
    vk_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vma_alloc_info = {};
    vma_alloc_info.
    vmaCreateBuffer(m_rhi->GetVMA(), &vk_create_info,  )

    VkBuffer staging_buf = VK_NULL_HANDLE;
    VkDeviceMemory staging_buf_mem = VK_NULL_HANDLE;
    CreateBuffer(
        buffer_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        staging_buf,
        staging_buf_mem);

    void* data;
    VK_VERIFY(vkMapMemory(m_vk_device, staging_buf_mem, 0, VK_WHOLE_SIZE, 0, &data));

    memcpy(data, vertices.data(), size_t(buffer_size));

    vkUnmapMemory(m_vk_device, staging_buf_mem);

    vkDestroyBuffer(m_vk_device, staging_buf, nullptr);
    vkFreeMemory(m_vk_device, staging_buf_mem, nullptr);
}

VulkanUploadBuffer::~VulkanUploadBuffer()
{
}

void VulkanUploadBuffer::AddRef()
{
}

void VulkanUploadBuffer::Release()
{
}
