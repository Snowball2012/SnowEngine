#include "StdAfx.h"

#include "Swapchain.h"

#include "D3D12RHIImpl.h"

IMPLEMENT_RHI_OBJECT(D3DSwapchain)

D3DSwapchain::D3DSwapchain(class D3D12RHI* rhi, const SwapChainCreateInfo& create_info)
	: m_rhi(rhi)
{
    /*
    DXGI_SWAP_CHAIN_DESC sd;
    sd.BufferDesc.Width = UINT(m_client_width);
    sd.BufferDesc.Height = UINT(m_client_height);
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferDesc.Format = m_back_buffer_format;
    sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = SwapChainBufferCount;
    sd.OutputWindow = m_main_hwnd;
    sd.Windowed = true;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    // Note: Swap chain uses queue to perform flush.
    ThrowIfFailedH(m_dxgi_factory->CreateSwapChain(
        m_gpu_device->GetGraphicsQueue()->GetNativeQueue(),
        &sd,
        m_swap_chain.GetAddressOf()));*/

	NOTIMPL;
}
