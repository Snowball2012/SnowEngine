#include "StdAfx.h"

#include "Rendergraph.h"

// RGTexture

RGTexture::RGTexture( uint64_t handle, const char* name, bool is_external )
    : RGResource( handle, name ), m_is_external( is_external )
{
}

// RGExternalTexture

RGExternalTexture::RGExternalTexture( uint64_t handle, const RGExternalTextureDesc& desc )
    : RGTexture( handle, desc.name, /*is_external=*/true ), m_desc( desc )
{
}

// Rendergraph

RGPass* Rendergraph::AddPass( RHI::QueueType queue_type, const char* name )
{
    std::unique_ptr<RGPass>& new_pass = m_passes.emplace_back( new RGPass( queue_type, name ) );

    return new_pass.get();
}

RGExternalTexture* Rendergraph::RegisterExternalTexture( const RGExternalTextureDesc& desc )
{
    std::unique_ptr<RGExternalTexture> ext_texture = std::make_unique<RGExternalTexture>( GenerateHandle(), desc );

    RGExternalTexture* texture_ptr = ext_texture.get();

    auto& entry = m_external_textures[ext_texture->GetHandle()];

    entry.texture = std::move( ext_texture );

    return texture_ptr;
}

RGResource* Rendergraph::CreateTransientTexture()
{
    NOTIMPL;
    return nullptr;
}

RGResource* Rendergraph::RegisterExternalAS()
{
    NOTIMPL;
    return nullptr;
}

RGResource* Rendergraph::CreateTransientBuffer()
{
    NOTIMPL;
    return nullptr;
}

namespace
{
    RHITextureLayout RGUsageToLayout( RGTextureUsage usage )
    {
        switch ( usage )
        {
        case RGTextureUsage::ShaderRead:
            return RHITextureLayout::ShaderReadOnly;
        case RGTextureUsage::ShaderReadWrite:
            return RHITextureLayout::ShaderReadWrite;
        case RGTextureUsage::RenderTarget:
            return RHITextureLayout::RenderTarget;
        }
        NOTIMPL;
    }
}

bool Rendergraph::Compile()
{
    // 0. Validate all passes are graphics
    for ( const auto& pass : m_passes )
    {
        if ( pass->m_queue_type != RHI::QueueType::Graphics )
        {
            NOTIMPL;
        }
    }

    // 1. Build layout transitions for external resources
    for ( const auto& pass : m_passes )
    {
        for ( const auto& [handle, used_texture] : pass->m_used_textures )
        {
            if ( !used_texture.texture->IsExternal() )
                continue;

            auto& ext_texture_entry = m_external_textures[handle];

            if ( !SE_ENSURE( ext_texture_entry.texture != nullptr ) )
                return false;

            if ( ext_texture_entry.first_usage == RGTextureUsage::Undefined )
            {
                ext_texture_entry.first_usage = used_texture.usage;
            }

            ext_texture_entry.last_usage = used_texture.usage;
        }
    }

    // there can be textures 

    std::vector<RHITextureBarrier> initial_barriers;
    m_final_barriers.clear();

    initial_barriers.reserve( m_external_textures.size() );
    m_final_barriers.reserve( m_external_textures.size() );

    for ( const auto& [handle, ext_texture_entry] : m_external_textures )
    {
        const RGExternalTextureDesc& tex_desc = ext_texture_entry.texture->GetDesc();

        // If texture is not really used in any of the passes, we still need to correctly transition it from initial layout to final layout
        if ( ext_texture_entry.first_usage == RGTextureUsage::Undefined )
        {
            SE_LOG_WARNING( Rendergraph, "External texture <%s> is added to the rendergraph but is not used in any pass!", ext_texture_entry.texture->GetName().c_str() );
        }

        RHITextureLayout required_first_layout =
            ( ext_texture_entry.first_usage == RGTextureUsage::Undefined )
                ? tex_desc.initial_layout
                : RGUsageToLayout( ext_texture_entry.first_usage );

        if ( required_first_layout != tex_desc.initial_layout )
        {
            auto& barrier = initial_barriers.emplace_back();
            barrier.layout_src = tex_desc.initial_layout;
            barrier.layout_dst = required_first_layout;
            barrier.texture = tex_desc.rhi_texture;
        }

        RHITextureLayout required_last_layout =
            ( ext_texture_entry.last_usage == RGTextureUsage::Undefined )
                ? tex_desc.initial_layout
                : RGUsageToLayout( ext_texture_entry.last_usage );

        if ( required_last_layout != tex_desc.final_layout )
        {
            auto& barrier = m_final_barriers.emplace_back();
            barrier.layout_src = required_last_layout;
            barrier.layout_dst = tex_desc.final_layout;
            barrier.texture = tex_desc.rhi_texture;
        }
    }

    // @todo - interface instead of raw friend access?
    m_passes.front()->m_pass_start_texture_barriers = std::move( initial_barriers );

    NOTIMPL;
    return false;
}

bool Rendergraph::Submit()
{
    NOTIMPL;
    for ( const auto& submission : m_submissions )
    {
        RHI::SubmitInfo rhi_submission = {};
        size_t cmd_lists_num = 0;
        for ( const auto* pass : submission.passes )
        {
            cmd_lists_num += pass->m_cmd_lists.size();
        }

        std::vector<RHICommandList*> cmd_lists;
        cmd_lists.reserve( cmd_lists_num );
        for ( const auto* pass : submission.passes )
        {
            cmd_lists.insert( cmd_lists.end(), pass->m_cmd_lists.begin(), pass->m_cmd_lists.end() );
        }

        rhi_submission.cmd_lists = cmd_lists.data();
        rhi_submission.cmd_list_count = cmd_lists.size();


        RHIFence fence = GetRHI().SubmitCommandLists( rhi_submission );
    }
    return true;
}

// RGPass

RGPass::RGPass( RHI::QueueType queue_type, const char* name )
    : m_queue_type( queue_type ), m_name( name )
{
}

bool RGPass::UseTexture( RGTexture& texture, RGTextureUsage usage )
{
    uint64_t key = texture.GetHandle();

    RGPassTexture& entry = m_used_textures[key];

    if ( !SE_ENSURE( ( entry.texture == nullptr ) || ( usage == entry.usage ) ) )
    {
        // we are trying to add a texture multiple times with different usage, that shouldn't happen
        return false;
    }

    entry.texture = &texture;
    entry.usage = usage;

    return true;
}

void RGPass::AddCommandList( RHICommandList& cmd_list )
{
    m_cmd_lists.emplace_back( &cmd_list );
}

void RGPass::EndPass()
{
    NOTIMPL;
}
