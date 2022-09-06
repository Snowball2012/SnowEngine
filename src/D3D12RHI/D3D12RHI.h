#pragma once

#ifdef D3D12RHI_EXPORTS
#define D3D12RHI_API __declspec(dllexport)
#else
#define D3D12RHI_API __declspec(dllimport)
#endif

#include <RHI/RHI.h>

struct D3D12RHICreateInfo
{
	void* main_window_handle = nullptr;
	const char* app_name = nullptr;
	bool enable_validation = false;
};

D3D12RHI_API class RHI* CreateD3D12RHI(const D3D12RHICreateInfo& info);

D3D12RHI_API void DestroyD3D12RHI(RHI* vulkan_rhi);

inline RHIPtr CreateD3D12RHI_RAII(const D3D12RHICreateInfo& info)
{
	return RHIPtr(CreateD3D12RHI(info), DestroyD3D12RHI);
}