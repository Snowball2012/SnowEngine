#include "StdAfx.h"

#include "Textures.h"

#include "VulkanRHIImpl.h"

VulkanTextureBase::VulkanTextureBase(VkImage image)
	: m_image(image)
{
}

IMPLEMENT_RHI_OBJECT(VulkanTexture)

VulkanTexture::VulkanTexture(VulkanRHI* rhi, const RHI::TextureInfo& info)
	:VulkanTextureBase(VK_NULL_HANDLE), m_rhi(rhi)
{
	VkImageCreateInfo image_info = {};

	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VulkanRHI::GetVkImageType(info.dimensions);
	image_info.extent.width = info.width;
	image_info.extent.height = info.height;
	image_info.extent.depth = info.depth;

	image_info.mipLevels = info.mips;
	image_info.arrayLayers = info.array_layers;

	image_info.format = VulkanRHI::GetVkFormat(info.format);
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;

	image_info.usage = VulkanRHI::GetVkImageUsageFlags(info.usage);
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.flags = 0;

	VmaAllocationCreateInfo vma_alloc_info = {};
	vma_alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	vma_alloc_info.flags = 0;
	VK_VERIFY(vmaCreateImage(m_rhi->GetVMA(), &image_info, &vma_alloc_info, &m_image, &m_allocation, nullptr));
}

VulkanTexture::~VulkanTexture()
{
	vmaDestroyImage(m_rhi->GetVMA(), m_image, m_allocation);
}
