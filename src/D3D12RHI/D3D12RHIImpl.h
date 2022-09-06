#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

class D3D12RHI : public RHI
{
private:
	ComPtr<IDXGIFactory4> m_dxgi_factory = nullptr;
	ComPtr<ID3D12Device5> m_d3d_device = nullptr;

	bool m_has_raytracing = false;

public:

	D3D12RHI(const struct D3D12RHICreateInfo& info);
	virtual ~D3D12RHI() override;

	// Inherited via RHI
	virtual void Present(RHISwapChain& swap_chain, const PresentInfo& info) override
	{
	}
	virtual void WaitIdle() override
	{
	}
	virtual RHIFence SubmitCommandLists(const SubmitInfo& info) override
	{
		return RHIFence{};
	}
	virtual void WaitForFenceCompletion(const RHIFence& fence) override
	{
	}

private:

	void CreateDevice(const D3D12RHICreateInfo& info);
};