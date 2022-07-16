#pragma once

#ifdef D3D12RHI_EXPORTS
#define D3D12RHI_API __declspec(dllexport)
#else
#define D3D12RHI_API __declspec(dllimport)
#endif

#include <RHI/RHI.h>

D3D12RHI_API class RHI* CreateD3D12RHI();

D3D12RHI_API void DestroyD3D12RHI(RHI* vulkan_rhi);

inline RHIPtr CreateD3D12RHI_RAII()
{
	return RHIPtr(CreateD3D12RHI(), DestroyD3D12RHI);
}