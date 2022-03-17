// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "RenderPass.h"


void RenderPass::Begin( RenderStateID state, IGraphicsCommandList& command_list ) noexcept
{
    assert( m_pso_cache.has( state ) );
    command_list.SetPipelineState( m_pso_cache[state].Get() );
    m_cmd_list = &command_list;
    BeginDerived( state );
}


void RenderPass::End() noexcept
{
    m_cmd_list = nullptr;
}


void RenderPass::DeleteState( RenderStateID state ) noexcept
{
    m_pso_cache.erase( state );
}
