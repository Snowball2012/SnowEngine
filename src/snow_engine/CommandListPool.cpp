// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "CommandListPool.h"


CommandList::CommandList( ID3D12Device& device, D3D12_COMMAND_LIST_TYPE type )
    : m_type( type )
{
    ThrowIfFailedH( device.CreateCommandAllocator( m_type, IID_PPV_ARGS( m_allocator.GetAddressOf() ) ) );
    ThrowIfFailedH( device.CreateCommandList( 0, type, m_allocator.Get(), nullptr, IID_PPV_ARGS( m_list.GetAddressOf() ) ) );
}


IGraphicsCommandList* CommandList::GetInterface() noexcept
{
    return m_list.Get();
}


const IGraphicsCommandList* CommandList::GetInterface() const noexcept
{
    return m_list.Get();
}


void CommandList::Reset( ID3D12PipelineState* initial_pso, bool reset_allocator )
{
    if ( reset_allocator )
        ThrowIfFailedH( m_allocator->Reset() );

    ThrowIfFailedH( m_list->Reset( m_allocator.Get(), initial_pso ) );
}


D3D12_COMMAND_LIST_TYPE CommandList::GetType() const noexcept
{
    return m_type;
}


CommandListPool::CommandListPool( ID3D12Device* device ) noexcept
    : m_device( device )
{
    assert( m_device );
}


CommandList CommandListPool::GetList( D3D12_COMMAND_LIST_TYPE type )
{
    auto& pool = m_pools[type];

    if ( ! pool.empty() )
    {
        auto list = std::move( pool.back() );
        pool.pop_back();
        return std::move( list );
    }

    auto new_list = CommandList( *m_device, type );
    ThrowIfFailedH( new_list.GetInterface()->Close() );
    return new_list;
}


void CommandListPool::PutList( CommandList&& list ) noexcept
{
    m_pools[list.GetType()].emplace_back( std::move( list ) );
}


void CommandListPool::Clear()
{
    for ( auto& pool : m_pools )
        pool.clear();
}
