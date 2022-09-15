#pragma once

#include <Core/RHICommon.h>

#include <vma/vk_mem_alloc.h>

// use this macro to define RHIObject body
#define GENERATE_RHI_OBJECT_BODY() \
protected: \
	class VulkanRHI* m_rhi = nullptr; \
	GENERATE_RHI_OBJECT_BODY_NO_RHI()

#define VK_VERIFY(x) VERIFY_EQUALS(x, VK_SUCCESS)