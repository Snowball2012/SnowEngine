#pragma once

#include <Core/RHICommon.h>

#include <dxgi1_5.h>
#include <d3d12.h>

#define HR_VERIFY(x) VERIFY_EQUALS(x, S_OK)

// use this macro to define RHIObject body
#define GENERATE_RHI_OBJECT_BODY() \
protected: \
	class D3D12RHI* m_rhi = nullptr; \
	GENERATE_RHI_OBJECT_BODY_NO_RHI()