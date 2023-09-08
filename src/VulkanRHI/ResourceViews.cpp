#include "StdAfx.h"

#include "ResourceViews.h"

#include "VulkanRHIImpl.h"

#include "Textures.h"

IMPLEMENT_RHI_OBJECT(VulkanCBV)

VulkanCBV::~VulkanCBV()
{
}

VulkanCBV::VulkanCBV(VulkanRHI* rhi, const RHIUniformBufferViewInfo& info)
	: m_rhi(rhi)
{
	VERIFY_NOT_EQUAL(info.buffer, nullptr);

	m_buffer = &RHIImpl(*info.buffer);

	m_view_info.buffer = m_buffer->GetVkBuffer();
	m_view_info.offset = 0;
	m_view_info.range = VK_WHOLE_SIZE;
}


IMPLEMENT_RHI_OBJECT(VulkanTextureSRV)

VulkanTextureSRV::~VulkanTextureSRV()
{
    if (m_vk_image_view)
        vkDestroyImageView(m_rhi->GetDevice(), m_vk_image_view, nullptr);
}

VulkanTextureSRV::VulkanTextureSRV(VulkanRHI* rhi, const RHI::TextureROViewInfo& info)
	: m_rhi(rhi)
{
	VERIFY_NOT_EQUAL(info.texture, nullptr);

	m_texture = &RHIImpl(*info.texture);

    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = m_texture->GetVkImage();
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = VulkanRHI::GetVkFormat(m_texture->GetInfo().format);
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = m_texture->GetInfo().mips;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = m_texture->GetInfo().array_layers;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    view_info.components =
    {
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY
    };

    VK_VERIFY(vkCreateImageView(m_rhi->GetDevice(), &view_info, nullptr, &m_vk_image_view));

	m_view_info.imageView = m_vk_image_view;
	m_view_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	m_view_info.sampler = nullptr;
}


IMPLEMENT_RHI_OBJECT( VulkanTextureRWView )

VulkanTextureRWView::~VulkanTextureRWView()
{
    if ( m_vk_image_view )
        vkDestroyImageView( m_rhi->GetDevice(), m_vk_image_view, nullptr );
}

VulkanTextureRWView::VulkanTextureRWView( VulkanRHI* rhi, const RHI::TextureRWViewInfo& info )
    : m_rhi( rhi )
{
    VERIFY_NOT_EQUAL( info.texture, nullptr );

    m_texture = &RHIImpl( *info.texture );

    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = m_texture->GetVkImage();
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;

    RHIFormat view_format = info.format == RHIFormat::Undefined ? m_texture->GetInfo().format : info.format;
    view_info.format = VulkanRHI::GetVkFormat( view_format );
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = m_texture->GetInfo().mips;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = m_texture->GetInfo().array_layers;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    view_info.components =
    {
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY
    };

    VK_VERIFY( vkCreateImageView( m_rhi->GetDevice(), &view_info, nullptr, &m_vk_image_view ) );

    m_view_info.imageView = m_vk_image_view;
    m_view_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    m_view_info.sampler = nullptr;
}


IMPLEMENT_RHI_OBJECT(VulkanRTV)

VulkanRTV::~VulkanRTV()
{
    if (m_vk_image_view)
        vkDestroyImageView(m_rhi->GetDevice(), m_vk_image_view, nullptr);
}

VulkanRTV::VulkanRTV(VulkanRHI* rhi, const RHI::RenderTargetViewInfo& info)
    : m_rhi(rhi)
{
    m_rhi_format = info.format;
    m_vk_image_view = m_rhi->CreateImageView(
        RHIImpl(info.texture)->GetVkImage(),
        VulkanRHI::GetVkFormat(info.format));
    VERIFY_NOT_EQUAL(info.texture, nullptr);

    m_texture = &RHIImpl(*info.texture);
}

glm::uvec3 VulkanRTV::GetSize() const
{
    const auto& tex_info = m_texture->GetInfo();
    return glm::uvec3(
        tex_info.width,
        tex_info.height,
        std::max<uint32_t>( tex_info.depth, 1 ) );
}

RHIFormat VulkanRTV::GetFormat() const
{
    return m_rhi_format;
}
