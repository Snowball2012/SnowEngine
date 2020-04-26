// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "RenderTask.h"

const RenderTask::Frustum& RenderTask::GetMainPassFrustum() const
{
    return m_main_frustum;
}

const span<const RenderTask::ShadowFrustum> RenderTask::GetShadowFrustums() const
{
    return make_span( m_shadow_frustums );
}

void RenderTask::AddShadowBatches( span<const RenderBatch> batches, uint32_t list_idx, uint32_t cascade_idx )
{
    if ( m_shadow_renderlists.size() < list_idx + 1 )
        m_shadow_renderlists.resize( list_idx + 1 );

	auto& shadow_cascades = m_shadow_renderlists[list_idx];
	if ( shadow_cascades.cascade_renderlists.size() < cascade_idx + 1 )
		shadow_cascades.cascade_renderlists.resize( cascade_idx + 1 );

    shadow_cascades.cascade_renderlists[cascade_idx].push_back( batches );
}

void RenderTask::AddOpaqueBatches( span<const RenderBatch> batches )
{
    m_opaque_renderlist.push_back( batches );
}

RenderTask::RenderTask( ForwardCBProvider&& forward_cb )
    : m_forward_cb_provider( std::move( forward_cb ) )
{
}
