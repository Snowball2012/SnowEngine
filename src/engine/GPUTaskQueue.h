#pragma once

#include "Ptr.h"

#include "CommandListPool.h"

#include "utils/span.h"

class GPUTaskQueue
{
public:
    using Timestamp = UINT64;

    GPUTaskQueue( ID3D12Device& device, D3D12_COMMAND_LIST_TYPE type, std::shared_ptr<CommandListPool> pool );
    ~GPUTaskQueue();

    D3D12_COMMAND_LIST_TYPE GetType() const noexcept { return m_type; }
    ID3D12CommandQueue* GetCmdQueue() noexcept;
    Timestamp CreateTimestamp();
    Timestamp GetLastPlacedTimestamp() const;

    void SubmitLists( const span<CommandList>& lists ) noexcept; // lists will be executed in the order of submission,
                                                                 // passed CommandLists will be invalided
                                                                 // lists must be closed
    Timestamp ExecuteSubmitted(); // calls ExecuteCommandLists

    // not const since 2 consequential calls may return different values
    Timestamp GetCurrentTimestamp();

    void WaitForTimestamp( Timestamp ts );

    // convinient equivalent of WaitForTimestamp( CreateTimestamp() )
    void Flush();

private:

    ComPtr<ID3D12CommandQueue> m_cmd_queue = nullptr;
    ComPtr<ID3D12Fence> m_fence = nullptr;
    UINT64 m_current_fence_value = 0;

    D3D12_COMMAND_LIST_TYPE m_type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    std::shared_ptr<CommandListPool> m_pool = nullptr;

    struct ScheduledList
    {
        CommandList list;
        Timestamp finish_time;
    };

    std::vector<CommandList> m_submitted_lists;
    std::vector<ID3D12CommandList*> m_submitted_interfaces; // for mem reuse

    std::queue<ScheduledList> m_scheduled_lists;

    void ReleaseProcessedLists( Timestamp ts );
};