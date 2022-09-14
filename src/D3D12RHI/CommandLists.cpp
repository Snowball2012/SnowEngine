#include "StdAfx.h"

#include "CommandLists.h"
#include <D3D12RHI/D3D12RHIImpl.h>

D3D12CommandListManager::D3D12CommandListManager(D3D12RHI* rhi)
	: m_rhi(rhi)
{
}

D3D12CommandListManager::~D3D12CommandListManager()
{
    WaitSubmittedUntilCompletion();

    for (const auto& queue_submitted_lists : m_submitted_lists)
    {
        assert(queue_submitted_lists.size() == 0);
    }

    for (auto& queue_cmd_lists : m_cmd_lists)
    {
        queue_cmd_lists.clear();
    }
}

D3D12CommandList* D3D12CommandListManager::GetCommandList(RHI::QueueType type)
{
    VERIFY_EQUALS(type < RHI::QueueType::Count, true);

    auto& free_lists = m_free_lists[size_t(type)];
    if (!free_lists.empty())
    {
        auto list_id = free_lists.back();
        free_lists.pop_back();
        return m_cmd_lists[size_t(type)][list_id].get();
    }

    CmdListId list_id = m_cmd_lists[size_t(type)].emplace();

    D3D12CommandList* new_list = new D3D12CommandList(m_rhi, type, list_id);
    m_cmd_lists[size_t(type)][list_id].reset(new_list);

    return new_list;
}

RHIFence D3D12CommandListManager::SubmitCommandLists(const RHI::SubmitInfo& info)
{
    if (info.cmd_list_count == 0)
        return RHIFence{};

    const size_t queue_idx = size_t(info.cmd_lists[0]->GetType());

    // Not necessary, but we have to do this somewhere at regular intervals. Why not here?
    ProcessCompleted();

    constexpr size_t typical_num_lists = 4;

    bc::small_vector<ID3D12CommandList*, typical_num_lists> cmd_lists;

    const size_t total_cmd_list_count = info.cmd_list_count;
    cmd_lists.reserve(total_cmd_list_count);

    for (size_t i = 0; i < info.cmd_list_count; ++i)
    {
        cmd_lists.emplace_back(RHIImpl(info.cmd_lists[i])->GetD3DCmdList());
    }

    // semaphore part is WIP
    {
        //NOTIMPL;
    }

    auto& submitted_lists = m_submitted_lists[queue_idx].emplace();

    submitted_lists.lists.reserve(total_cmd_list_count);

    for (size_t i = 0; i < info.cmd_list_count; ++i)
        submitted_lists.lists.emplace_back(RHIImpl(*(info.cmd_lists + i)));

    D3DQueue* d3d_queue = m_rhi->GetQueue(info.cmd_lists[0]->GetType());
    VERIFY_NOT_EQUAL(d3d_queue, nullptr);

    d3d_queue->GetD3DCommandQueue()->ExecuteCommandLists(UINT(cmd_lists.size()), cmd_lists.data());
    uint64_t fence_value = d3d_queue->InsertSignal();
    submitted_lists.completion_fence = fence_value;

    return RHIFence{
        submitted_lists.completion_fence,
        0,
        static_cast<uint64_t>(info.cmd_lists[0]->GetType()),
    };
}

void D3D12CommandListManager::WaitForFence(const RHIFence& fence)
{
    RHI::QueueType queue_type = static_cast<RHI::QueueType>(fence._3);
    uint64_t signal_value = fence._1;

    D3DQueue* queue = m_rhi->GetQueue(queue_type);
    if (!queue)
        return;

    queue->WaitForSignal(signal_value);
}

void D3D12CommandListManager::ProcessCompleted()
{
    for (size_t queue_i = 0; queue_i < m_submitted_lists.size(); ++queue_i)
    {
        auto& submitted_lists = m_submitted_lists[queue_i];
        auto& free_lists = m_free_lists[queue_i];
        D3DQueue* d3d_queue = m_rhi->GetQueue(RHI::QueueType(queue_i));
        VERIFY_NOT_EQUAL(d3d_queue, nullptr);

        uint64_t fence_current_value = d3d_queue->Poll();

        while (!submitted_lists.empty())
        {
            auto& submitted_info = submitted_lists.front();
            if (submitted_info.completion_fence > fence_current_value)
                break;

            for (D3D12CommandList* list : submitted_info.lists)
                free_lists.emplace_back(list->GetListId());

            submitted_lists.pop();
        }
    }
}

void D3D12CommandListManager::WaitSubmittedUntilCompletion()
{
    for (size_t queue_i = 0; queue_i < m_submitted_lists.size(); ++queue_i)
    {
        auto& submitted_lists = m_submitted_lists[queue_i];

        if (submitted_lists.empty())
            continue;

        D3DQueue* d3d_queue = m_rhi->GetQueue(RHI::QueueType(queue_i));
        VERIFY_NOT_EQUAL(d3d_queue, nullptr);

        d3d_queue->WaitForSignal(submitted_lists.back().completion_fence);
    }

    ProcessCompleted();
}

D3D12CommandList::D3D12CommandList(D3D12RHI* rhi, RHI::QueueType type, CmdListId list_id)
    : m_rhi(rhi), m_type(type), m_list_id(list_id)
{
    auto* device = rhi->GetDevice();
    auto d3d_type = D3D12RHI::GetD3DCommandListType(type);
    HR_VERIFY(device->CreateCommandAllocator(d3d_type, IID_PPV_ARGS(m_d3d_cmd_allocator.GetAddressOf())));
    HR_VERIFY(device->CreateCommandList(0, d3d_type, m_d3d_cmd_allocator.Get(), nullptr, IID_PPV_ARGS(m_d3d_cmd_list.GetAddressOf())));

    // command list was created in open state, close it
    End();
}

D3D12CommandList::~D3D12CommandList()
{
}

RHI::QueueType D3D12CommandList::GetType() const
{
    return m_type;
}

void D3D12CommandList::Begin()
{
    Reset();
}

void D3D12CommandList::End()
{
    HR_VERIFY(m_d3d_cmd_list->Close());
}

void D3D12CommandList::Reset()
{
    HR_VERIFY(m_d3d_cmd_allocator->Reset());
    HR_VERIFY(m_d3d_cmd_list->Reset(m_d3d_cmd_allocator.Get(), nullptr));
}
