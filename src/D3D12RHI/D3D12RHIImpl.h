#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

class D3DQueue
{
private:
	class D3D12RHI* m_rhi = nullptr;

	ComPtr<ID3D12CommandQueue> m_d3d_queue = nullptr;

	ComPtr<ID3D12Fence> m_fence = nullptr;

	UINT64 m_last_submitted_value = 0;

public:
	D3DQueue(class D3D12RHI* rhi, D3D12_COMMAND_LIST_TYPE type);

	ID3D12CommandQueue* GetD3DCommandQueue() const { return m_d3d_queue.Get(); }

	uint64_t InsertSignal();
	void WaitForSignal(uint64_t signal);

	uint64_t Poll();

	void Flush();
};

class D3D12RHI : public RHI
{
private:
	// device
	ComPtr<IDXGIFactory4> m_dxgi_factory = nullptr;
	ComPtr<ID3D12Device5> m_d3d_device = nullptr;

	bool m_has_raytracing = false;

	// queues
	std::unique_ptr<D3DQueue> m_graphics_queue = nullptr;

	// command lists
	std::unique_ptr<class D3D12CommandListManager> m_cmd_list_mgr = nullptr;

	RHIObjectPtr<class D3DSwapchain> m_main_swapchain = nullptr;

public:

	D3D12RHI(const struct D3D12RHICreateInfo& info);
	virtual ~D3D12RHI() override;

	// Inherited via RHI
	virtual void Present(RHISwapChain& swap_chain, const PresentInfo& info) override
	{
		NOTIMPL;
	}
	virtual void WaitIdle() override;

	virtual RHIFence SubmitCommandLists(const SubmitInfo& info) override;
	virtual void WaitForFenceCompletion(const RHIFence& fence) override;

	virtual RHICommandList* GetCommandList(QueueType type);

	ID3D12Device* GetDevice() const { return m_d3d_device.Get(); }

	D3DQueue* GetQueue(RHI::QueueType type) const;

	static D3D12_COMMAND_LIST_TYPE GetD3DCommandListType(RHI::QueueType type);

	void DeferredDestroyRHIObject(RHIObject* obj);

private:

	class D3DSwapchain* CreateSwapChainInternal(const RHISwapChainCreateInfo& create_info);

	void CreateDevice(const D3D12RHICreateInfo& info);

	void CreateQueues();
};