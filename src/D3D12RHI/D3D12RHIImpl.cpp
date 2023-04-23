#include "StdAfx.h"

#include "D3D12RHIImpl.h"

#include "D3D12RHI.h"

#include "CommandLists.h"

#include "Swapchain.h"

D3D12RHI::D3D12RHI(const D3D12RHICreateInfo& info)
{
	CreateDevice(info);
    CreateQueues();

    m_cmd_list_mgr = std::make_unique<D3D12CommandListManager>(this);


    RHISwapChainCreateInfo swapchain_create_info = {};
    swapchain_create_info.surface_num = 2;
    swapchain_create_info.window_handle = info.main_window_handle;

    m_main_swapchain = CreateSwapChainInternal(swapchain_create_info);
}

D3D12RHI::~D3D12RHI()
{
	std::cout << "D3D12 RHI shutdown started" << std::endl;

    WaitIdle();

    m_cmd_list_mgr = nullptr;

    m_graphics_queue = nullptr;
    m_d3d_device = nullptr;
    m_dxgi_factory = nullptr;

    std::cout << "D3D12 RHI shutdown completed" << std::endl;
}

void D3D12RHI::WaitIdle()
{
    if (m_graphics_queue)
        m_graphics_queue->Flush();
}

RHIFence D3D12RHI::SubmitCommandLists(const SubmitInfo& info)
{
    return m_cmd_list_mgr->SubmitCommandLists(info);
}

void D3D12RHI::WaitForFenceCompletion(const RHIFence& fence)
{
    return m_cmd_list_mgr->WaitForFence(fence);
}

RHICommandList* D3D12RHI::GetCommandList(QueueType type)
{
    return m_cmd_list_mgr->GetCommandList(type);
}

D3DQueue* D3D12RHI::GetQueue(RHI::QueueType type) const
{
    switch (type)
    {
    case RHI::QueueType::Graphics:
        return m_graphics_queue.get();
    }

    NOTIMPL;
}

D3D12_COMMAND_LIST_TYPE D3D12RHI::GetD3DCommandListType(RHI::QueueType type)
{
    switch (type)
    {
    case RHI::QueueType::Graphics:
        return D3D12_COMMAND_LIST_TYPE_DIRECT;
    }

    NOTIMPL;
}

void D3D12RHI::DeferredDestroyRHIObject(RHIObject* obj)
{
    NOTIMPL;
}

D3DSwapchain* D3D12RHI::CreateSwapChainInternal(const RHISwapChainCreateInfo& create_info)
{
    return new D3DSwapchain(this, create_info);
}

void D3D12RHI::CreateDevice(const D3D12RHICreateInfo& info)
{
	if (info.enable_validation)
	{
		ComPtr<ID3D12Debug> debug_controller0;
		ComPtr<ID3D12Debug1> debug_controller1;
		HR_VERIFY(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller0)));
		debug_controller0->EnableDebugLayer();
		HR_VERIFY(debug_controller0->QueryInterface(IID_PPV_ARGS(&debug_controller1)));
		debug_controller1->SetEnableGPUBasedValidation(true);
	}
	std::cout << "D3D12RHI: validation " << (info.enable_validation ? "enabled" : "disabled") << std::endl;

    HR_VERIFY(CreateDXGIFactory1(IID_PPV_ARGS(&m_dxgi_factory)));

    // Try to create hardware device.
    HRESULT hardware_result;
    hardware_result = D3D12CreateDevice(
        nullptr,             // default adapter
        D3D_FEATURE_LEVEL_12_1,
        IID_PPV_ARGS(&m_d3d_device));

    // Fallback to WARP device.
    if (FAILED(hardware_result))
    {
        ComPtr<IDXGIAdapter> warp_adapter;
        HR_VERIFY(m_dxgi_factory->EnumWarpAdapter(IID_PPV_ARGS(&warp_adapter)));

        std::cout << "D3D12RHI: failed to create a D3D12 device, fallback to WARP" << std::endl;


        HR_VERIFY(D3D12CreateDevice(
            warp_adapter.Get(),
            D3D_FEATURE_LEVEL_12_1,
            IID_PPV_ARGS(&m_d3d_device)));
    }

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 features = {};
    HR_VERIFY(m_d3d_device->CheckFeatureSupport(
        D3D12_FEATURE_D3D12_OPTIONS5,
        &features, sizeof(features)));

    m_has_raytracing = features.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0;

    std::cout << "D3D12RHI: raytracing " << (m_has_raytracing ? "supported" : "not supported") << std::endl;
}

void D3D12RHI::CreateQueues()
{
    m_graphics_queue = std::make_unique<D3DQueue>(this, D3D12_COMMAND_LIST_TYPE_DIRECT);
}

D3DQueue::D3DQueue(D3D12RHI* rhi, D3D12_COMMAND_LIST_TYPE type)
    : m_rhi(rhi)
{
    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Type = type;
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    HR_VERIFY(m_rhi->GetDevice()->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_d3d_queue)));
    HR_VERIFY(m_rhi->GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
}

uint64_t D3DQueue::InsertSignal()
{
    HR_VERIFY(m_d3d_queue->Signal(m_fence.Get(), ++m_last_submitted_value));

    return m_last_submitted_value;
}

void D3DQueue::WaitForSignal(uint64_t signal)
{
    if (m_fence->GetCompletedValue() < signal)
    {
        HANDLE event = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);

        VERIFY_NOT_EQUAL(event, nullptr);

        // Fire event when GPU hits current fence.  
        HR_VERIFY(m_fence->SetEventOnCompletion(signal, event));

        // Wait until the GPU hits current fence event is fired.
        WaitForSingleObject(event, INFINITE);
        CloseHandle(event);
    }
}

uint64_t D3DQueue::Poll()
{
    return m_fence->GetCompletedValue();
}

void D3DQueue::Flush()
{
    WaitForSignal(InsertSignal());
}
