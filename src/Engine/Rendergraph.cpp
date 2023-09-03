#include "StdAfx.h"

#include "Rendergraph.h"

#include "DescriptorSetPool.h"

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

Rendergraph::Rendergraph()
{
    m_descriptors = std::make_unique<DescriptorSetPool>();
    m_upload_buffers = std::make_unique<UploadBufferPool>();
}

Rendergraph::~Rendergraph() = default;

RGPass* Rendergraph::AddPass( RHI::QueueType queue_type, const char* name )
{
    std::unique_ptr<RGPass>& new_pass = m_passes.emplace_back( new RGPass( queue_type, name ) );

    return new_pass.get();
}

RGExternalTexture* Rendergraph::RegisterExternalTexture( const RGExternalTextureDesc& desc )
{
    if ( !SE_ENSURE( desc.rhi_texture != nullptr ) )
        return nullptr;

    std::unique_ptr<RGExternalTexture> ext_texture = std::make_unique<RGExternalTexture>( GenerateHandle(), desc );

    ext_texture->SetRHITexture( desc.rhi_texture );

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

    std::vector<RHITextureBarrier> initial_barriers;
    m_final_barriers.clear();

    initial_barriers.reserve( m_external_textures.size() );
    m_final_barriers.reserve( m_external_textures.size() );

    std::unordered_map<uint64_t, RHITextureLayout> current_layouts;

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

        current_layouts[handle] = required_first_layout;
    }

    // @todo - interface instead of raw friend access?
    m_passes.front()->m_pass_start_texture_barriers = std::move( initial_barriers );

    // Build all other layout transitions
    for ( size_t i = 1; i < m_passes.size(); i++ )
    {
        std::vector<RHITextureBarrier> pass_barriers;

        const auto& used_textures = m_passes[i]->m_used_textures;

        for ( const auto& used_texture : used_textures )
        {
            auto curr_layout_it = current_layouts.find( used_texture.first );

            RHITextureLayout curr_layout = RHITextureLayout::Undefined;
            RHITextureLayout new_layout = RGUsageToLayout( used_texture.second.usage );
            if ( curr_layout_it == current_layouts.end() )
            {
                curr_layout = new_layout;
                current_layouts[used_texture.first] = new_layout;
            }
            else
            {
                curr_layout = curr_layout_it->second;
                curr_layout_it->second = new_layout;
            }

            if ( curr_layout != new_layout )
            {
                auto& barrier = pass_barriers.emplace_back();
                barrier.layout_src = curr_layout;
                barrier.layout_dst = new_layout;
                barrier.texture = used_texture.second.texture->GetRHITexture();
            }
        }

        m_passes[i]->m_pass_start_texture_barriers = std::move( pass_barriers );
        pass_barriers.clear();
    }

    // Make submissions
    m_submissions.resize( 1 );
    {
        auto& submission = m_submissions.front();
        submission.passes.reserve( m_passes.size() );
        for ( const auto& pass : m_passes )
        {
            submission.passes.emplace_back( pass.get() );
        }
        submission.type = RHI::QueueType::Graphics;
    }

    return true;
}

RHIFence Rendergraph::Submit( const RGSubmitInfo& info )
{
    if ( m_submissions.empty() && !m_final_barriers.empty() )
    {
        // We still need one submission for barriers
        m_submissions.emplace_back();
    }

    m_submissions.back().end_barriers = std::move( m_final_barriers );

    if ( m_submissions.size() > 1 )
    {
        // need to synchronize multiple submissions
        NOTIMPL;
    }

    RHIFence last_fence = {};

    for ( const auto& submission : m_submissions )
    {
        RHI::SubmitInfo rhi_submission = {};
        size_t cmd_lists_num = 0;
        for ( const auto* pass : submission.passes )
        {
            cmd_lists_num += pass->m_cmd_lists.size();
        }

        if ( !submission.end_barriers.empty() )
        {
            cmd_lists_num++;
        }

        std::vector<RHICommandList*> cmd_lists;
        cmd_lists.reserve( cmd_lists_num );
        for ( const auto* pass : submission.passes )
        {
            cmd_lists.insert( cmd_lists.end(), pass->m_cmd_lists.begin(), pass->m_cmd_lists.end() );
        }

        if ( !submission.end_barriers.empty() )
        {
            RHICommandList* end_barriers_cmd_list = GetRHI().GetCommandList( submission.type );
            end_barriers_cmd_list->Begin();
            end_barriers_cmd_list->TextureBarriers( submission.end_barriers.data(), submission.end_barriers.size() );
            end_barriers_cmd_list->End();
            cmd_lists.emplace_back( end_barriers_cmd_list );
        }

        rhi_submission.cmd_lists = cmd_lists.data();
        rhi_submission.cmd_list_count = cmd_lists.size();

        rhi_submission.wait_semaphore_count = info.wait_semaphore_count;
        rhi_submission.semaphores_to_wait = info.semaphores_to_wait;
        rhi_submission.stages_to_wait = info.stages_to_wait;
        rhi_submission.semaphore_to_signal = info.semaphore_to_signal;

        last_fence = GetRHI().SubmitCommandLists( rhi_submission );
    }
    return last_fence;
}

void Rendergraph::Reset()
{
    m_passes.clear();
    m_submissions.clear();
    m_external_textures.clear();
    m_final_barriers.clear();
    m_descriptors->Reset();
    m_upload_buffers->Reset();
}

RHIDescriptorSet* Rendergraph::AllocateFrameDescSet( RHIDescriptorSetLayout& layout )
{
    return m_descriptors->Allocate( layout );
}

UploadBufferRange Rendergraph::AllocateUploadBuffer( size_t size )
{
    return m_upload_buffers->Allocate( size );
}

// RGPass

RGPass::RGPass( RHI::QueueType queue_type, const char* name )
    : m_queue_type( queue_type ), m_name( name )
{
}

bool RGPass::UseTexture( const RGTexture& texture, RGTextureUsage usage )
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
    if ( m_cmd_lists.empty() && ( m_borrowed_list == false ) )
    {
        AddPassBeginCommands( cmd_list );
    }

    m_cmd_lists.emplace_back( &cmd_list );
}

bool RGPass::BorrowCommandList( RHICommandList& cmd_list )
{
    if ( !SE_ENSURE( m_borrowed_list == false ) )
        return false;

    m_borrowed_list = true;
    AddPassBeginCommands( cmd_list );
    return true;
}

void RGPass::EndPass()
{
    // nothing to do?
}

void RGPass::AddPassBeginCommands( RHICommandList& cmd_list )
{
    cmd_list.TextureBarriers( m_pass_start_texture_barriers.data(), m_pass_start_texture_barriers.size() );
}
