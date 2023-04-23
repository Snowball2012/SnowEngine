#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

#include "Buffers.h"

class VulkanAccelerationStructure : public RHIAccelerationStructure
{
	GENERATE_RHI_OBJECT_BODY()

	VulkanBufferPtr m_underlying_buffer = nullptr;

	VkAccelerationStructureKHR m_vk_as = VK_NULL_HANDLE;

	VkAccelerationStructureTypeKHR m_vk_type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

public:
	VulkanAccelerationStructure( VulkanRHI* rhi, const RHI::ASInfo& info );

	virtual ~VulkanAccelerationStructure() override;

	VkAccelerationStructureKHR GetVkAS() const { return m_vk_as; }

	VkAccelerationStructureTypeKHR GetVkType() const { return m_vk_type; }
};
IMPLEMENT_RHI_INTERFACE( RHIAccelerationStructure, VulkanAccelerationStructure )