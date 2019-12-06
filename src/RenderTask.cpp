// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "RenderTask.h"

const RenderTask::Frustrum& RenderTask::GetMainPassFrustrum() const
{
    return m_main_frustrum;
}

const span<const RenderTask::ShadowFrustrum> RenderTask::GetShadowFrustrums() const
{
    return make_span( m_shadow_frustrums );
}

void RenderTask::AddShadowBatches( span<const RenderBatch> batches, uint32_t list_idx )
{
    if ( m_shadow_renderlists.size() < list_idx + 1 )
        m_shadow_renderlists.resize( list_idx + 1 );

    m_shadow_renderlists[list_idx].push_back( batches );
}

void RenderTask::AddOpaqueBatches( span<const RenderBatch> batches )
{
    m_opaque_renderlist.push_back( batches );
}

RenderTask::RenderTask( ForwardCBProvider&& forward_cb )
    : m_forward_cb_provider( std::move( forward_cb ) )
{
}
