#include "StdAfx.h"

#include "D3D12RHIImpl.h"

#include "D3D12RHI.h"

D3D12RHI::D3D12RHI(const D3D12RHICreateInfo& info)
{
	CreateDevice(info);
}

D3D12RHI::~D3D12RHI()
{
	std::cout << "D3D12 RHI shutdown" << std::endl;
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
