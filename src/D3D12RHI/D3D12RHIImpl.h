#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

class D3D12RHI : public RHI
{
public:
	D3D12RHI();
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
};