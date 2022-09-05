#include "StdAfx.h"

#include "D3D12RHI.h"

#include "D3D12RHIImpl.h"

D3D12RHI_API RHI* CreateD3D12RHI()
{
	return new D3D12RHI();
}

D3D12RHI_API void DestroyD3D12RHI(RHI* d3d12_rhi)
{
	if (d3d12_rhi)
		delete d3d12_rhi;
}
