#pragma once

#include "Ptr.h"

#include <d3d12.h>


class CommandList
{
public:
    CommandList( ID3D12Device& device, D3D12_COMMAND_LIST_TYPE type );

    CommandList( const CommandList& ) = delete;
    CommandList& operator=( const CommandList& ) = delete;
    CommandList( CommandList&& ) = default;
    CommandList& operator=( CommandList&& ) = default;

    IGraphicsCommandList* GetInterface() noexcept;
    const IGraphicsCommandList* GetInterface() const noexcept;
    void Reset( ID3D12PipelineState* initial_pso = nullptr, bool reset_allocator = true );
    D3D12_COMMAND_LIST_TYPE GetType() const noexcept;

private:
    ComPtr<ID3D12CommandAllocator> m_allocator;
    ComPtr<IGraphicsCommandList> m_list;
    D3D12_COMMAND_LIST_TYPE m_type;
};


// not thread-safe
class CommandListPool
{
public:
    CommandListPool( ID3D12Device* device ) noexcept;

    CommandListPool( const CommandListPool& ) = delete;
    CommandListPool& operator=( const CommandListPool& ) = delete;
    CommandListPool( CommandListPool&& ) noexcept = default;
    CommandListPool& operator=( CommandListPool&& ) noexcept = default;

    CommandList GetList( D3D12_COMMAND_LIST_TYPE type ); // list here is in closed state ( before Reset )
    void PutList( CommandList&& list ) noexcept;         // list must be closed

    void Clear();

private:
    static constexpr uint32_t NumAllocatorTypes = 6; // see D3D12_COMMAND_LIST_TYPE enum in d3d12.h

    std::array<std::vector<CommandList>, NumAllocatorTypes> m_pools;

    ID3D12Device* m_device;
};