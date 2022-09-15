#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

class D3DSwapchain : public RHISwapChain
{
	GENERATE_RHI_OBJECT_BODY()

private:
	ComPtr<IDXGISwapChain> m_swapchain = nullptr;

public:
	D3DSwapchain(class D3D12RHI* rhi, const SwapChainCreateInfo& create_info);

	virtual ~D3DSwapchain() override {}

	virtual glm::uvec2 GetExtent() const override { NOTIMPL; }
	virtual void AcquireNextImage(class RHISemaphore* semaphore_to_signal, bool& out_recreated) override { NOTIMPL; }
	virtual void Recreate() override { NOTIMPL; }
	virtual RHIFormat GetFormat() const override { NOTIMPL; }
	virtual RHITexture* GetTexture() override { NOTIMPL; }

	virtual RHIRTV* GetRTV() override { NOTIMPL; }
};