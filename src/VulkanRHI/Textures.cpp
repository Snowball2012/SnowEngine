#include "StdAfx.h"

#include "Textures.h"

#include "VulkanRHIImpl.h"

VulkanTextureBase::VulkanTextureBase(VkImage image, const RHI::TextureInfo& info)
	: m_image(image), m_info(info)
{
}

IMPLEMENT_RHI_OBJECT(VulkanTexture)

VulkanTexture::VulkanTexture(VulkanRHI* rhi, const RHI::TextureInfo& info)
	:VulkanTextureBase(VK_NULL_HANDLE, info), m_rhi(rhi)
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
	if ( info.allow_multiformat_views )
	{
		image_info.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
	}

	VmaAllocationCreateInfo vma_alloc_info = {};
	vma_alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	vma_alloc_info.flags = 0;
	VK_VERIFY(vmaCreateImage(m_rhi->GetVMA(), &image_info, &vma_alloc_info, &m_image, &m_allocation, nullptr));

	VkImageLayout desired_layout = VulkanRHI::GetVkImageLayout(info.initial_layout);
	if (desired_layout != VK_IMAGE_LAYOUT_UNDEFINED)
	{
		// image has to be created in specified layout, but Vulkan only allows for VK_IMAGE_LAYOUT_UNDEFINED and VK_IMAGE_LAYOUT_PREINITIALIZED
		// insert deferred layout transition here. it will happen at the next SubmitCommandLists
		m_rhi->DeferImageLayoutTransition(m_image, info.initial_queue, VK_IMAGE_LAYOUT_UNDEFINED, desired_layout);
	}
}

VulkanTexture::~VulkanTexture()
{
	vmaDestroyImage(m_rhi->GetVMA(), m_image, m_allocation);
}


IMPLEMENT_RHI_OBJECT(VulkanSampler)

VulkanSampler::VulkanSampler(VulkanRHI* rhi, const RHI::SamplerInfo& info)
	: m_rhi(rhi)
{
	VkSamplerCreateInfo sampler_info = {};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

	if (info.enable_anisotropy)
	{
		sampler_info.anisotropyEnable = VK_TRUE;
		sampler_info.maxAnisotropy = std::min<float>(8.0f, m_rhi->GetPhysDeviceProps().limits.maxSamplerAnisotropy);
	}
	else
	{
		sampler_info.anisotropyEnable = VK_FALSE;
	}
	sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_info.mipLodBias = info.mip_bias;

	VK_VERIFY(vkCreateSampler(m_rhi->GetDevice(), &sampler_info, nullptr, &m_sampler));

	m_desc_info.sampler = m_sampler;
}

VulkanSampler::~VulkanSampler()
{
	if (m_sampler)
		vkDestroySampler(m_rhi->GetDevice(), m_sampler, nullptr);
}
