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

	VkDeviceAddress m_as_device_address = 0;

public:
	VulkanAccelerationStructure( VulkanRHI* rhi, const RHI::ASInfo& info );

	virtual ~VulkanAccelerationStructure() override;

	VkAccelerationStructureKHR GetVkAS() const { return m_vk_as; }
	const VkAccelerationStructureKHR* GetVkASPtr() const { return &m_vk_as; }

	VkDeviceAddress GetVkASDeviceAddress() const { return m_as_device_address; }

	VkAccelerationStructureTypeKHR GetVkType() const { return m_vk_type; }

	size_t GetSize() const { return m_underlying_buffer->GetSize(); }
};
IMPLEMENT_RHI_INTERFACE( RHIAccelerationStructure, VulkanAccelerationStructure )

class VulkanASInstanceBuffer : public RHIASInstanceBuffer
{
	GENERATE_RHI_OBJECT_BODY()

	VulkanUploadBufferPtr m_gpu_buf = nullptr;

public:
	VulkanASInstanceBuffer( VulkanRHI* rhi, size_t initial_instances_num );

	virtual ~VulkanASInstanceBuffer() override;

	virtual void UpdateBuffer( const RHIASInstanceData* data, size_t num_instances ) override;

	VkDeviceAddress GetVkDeviceAddress() const;

private:
	static size_t CalcRequiredMem( size_t num_instances ) { return num_instances * sizeof( VkAccelerationStructureInstanceKHR ); }

	void Init( size_t size );
};
IMPLEMENT_RHI_INTERFACE( RHIASInstanceBuffer, VulkanASInstanceBuffer )