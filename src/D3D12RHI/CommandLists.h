#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

class D3D12CommandListManager
{
private:
	class D3D12RHI* m_rhi = nullptr;

	std::array<packed_freelist<std::unique_ptr<class D3D12CommandList>>, size_t(RHI::QueueType::Count)> m_cmd_lists;

public:
	using CmdListId = std::remove_reference_t<decltype(m_cmd_lists[0])>::id;

private:

	std::array<std::vector<CmdListId>, size_t(RHI::QueueType::Count)> m_free_lists;

	struct SubmittedListsInfo
	{
		std::vector<D3D12CommandList*> lists;
		uint64_t completion_fence = 0;
	};
	std::array<std::queue<SubmittedListsInfo>, size_t(RHI::QueueType::Count)> m_submitted_lists;

public:
	D3D12CommandListManager(D3D12RHI* rhi);
	~D3D12CommandListManager();

	D3D12CommandList* GetCommandList(RHI::QueueType type);
	RHIFence SubmitCommandLists(const RHI::SubmitInfo& info);

	void WaitForFence(const RHIFence& fence);

	void ProcessCompleted();

	void WaitSubmittedUntilCompletion();

};

class D3D12CommandList : public RHICommandList
{
public:
	using CmdListId = D3D12CommandListManager::CmdListId;

private:

	ComPtr<ID3D12GraphicsCommandList> m_d3d_cmd_list = nullptr;
	ComPtr<ID3D12CommandAllocator> m_d3d_cmd_allocator = nullptr;
	RHI::QueueType m_type = RHI::QueueType::Graphics;
	CmdListId m_list_id = CmdListId::nullid;
	class D3D12RHI* m_rhi = nullptr;

public:
	D3D12CommandList(D3D12RHI* rhi, RHI::QueueType type, CmdListId list_id);
	virtual ~D3D12CommandList();

	virtual RHI::QueueType GetType() const override;

	virtual void Begin() override;
	virtual void End() override;

	CmdListId GetListId() const { return m_list_id; }

	ID3D12GraphicsCommandList* GetD3DCmdList() const { return m_d3d_cmd_list.Get(); }

	void Reset();
};

IMPLEMENT_RHI_INTERFACE(RHICommandList, D3D12CommandList)