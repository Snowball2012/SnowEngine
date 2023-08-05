#include "StdAfx.h"

#include "Rendergraph.h"

// RGTexture

RGTexture::RGTexture( uint64_t handle, const char* name )
    : RGResource( handle, name )
{
}

// RGExternalTexture

RGExternalTexture::RGExternalTexture( uint64_t handle, const RGExternalTextureDesc& desc )
    : RGTexture( handle, desc.name ), m_desc( desc )
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

    m_external_textures.emplace_back( std::move( ext_texture ) );

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

bool Rendergraph::Compile()
{
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
