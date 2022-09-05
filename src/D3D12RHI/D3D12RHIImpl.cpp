#include "StdAfx.h"

#include "D3D12RHIImpl.h"

D3D12RHI::D3D12RHI()
{
	std::cout << "D3D12 RHI initialization" << std::endl;
}

D3D12RHI::~D3D12RHI()
{
	std::cout << "D3D12 RHI shutdown" << std::endl;
}
