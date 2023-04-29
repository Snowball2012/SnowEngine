#include "StdAfx.h"

#include "AccelerationStructures.h"

#include "VulkanRHIImpl.h"

IMPLEMENT_RHI_OBJECT( VulkanAccelerationStructure )

VulkanAccelerationStructure::VulkanAccelerationStructure( VulkanRHI* rhi, const RHI::ASInfo& info )
    : m_rhi( rhi )
{
    RHI::BufferInfo buf_info = {};
    buf_info.size = info.size;
    buf_info.usage = RHIBufferUsageFlags::AccelerationStructure;
    m_underlying_buffer = RHIImpl( m_rhi->CreateDeviceBuffer( buf_info ) );

    m_vk_type = VulkanRHI::GetVkASType( info.type );

    VkAccelerationStructureCreateInfoKHR vk_ci = {};
    vk_ci.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    vk_ci.type = m_vk_type;
    vk_ci.buffer = m_underlying_buffer->GetVkBuffer();
    VK_VERIFY( vkCreateAccelerationStructureKHR( m_rhi->GetDevice(), &vk_ci, nullptr, &m_vk_as ) );

    VkAccelerationStructureDeviceAddressInfoKHR dev_addr_info = {};
    dev_addr_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    dev_addr_info.accelerationStructure = m_vk_as;
    m_as_device_address = vkGetAccelerationStructureDeviceAddressKHR( m_rhi->GetDevice(), &dev_addr_info );
}

VulkanAccelerationStructure::~VulkanAccelerationStructure()
{
    vkDestroyAccelerationStructureKHR( m_rhi->GetDevice(), m_vk_as, nullptr);
}


IMPLEMENT_RHI_OBJECT( VulkanASInstanceBuffer )

VulkanASInstanceBuffer::VulkanASInstanceBuffer( VulkanRHI* rhi, size_t initial_instances_num )
    : m_rhi( rhi )
{
    Init( CalcRequiredMem( initial_instances_num ) );
}

VulkanASInstanceBuffer::~VulkanASInstanceBuffer()
{
}

void VulkanASInstanceBuffer::UpdateBuffer( const RHIASInstanceData* data, size_t num_instances )
{
    size_t required_mem = CalcRequiredMem( num_instances );
    if ( m_gpu_buf->GetBuffer()->GetSize() < required_mem )
    {
        Init( required_mem + required_mem / 4 );
    }

    std::vector<VkAccelerationStructureInstanceKHR> vk_cpu_data( num_instances );
    for ( size_t i = 0; i < num_instances; ++i )
    {
        auto& vk_instance_data = vk_cpu_data[i];
        const auto& rhi_instance = data[i];

        vk_instance_data = {};
        vk_instance_data.mask = MASK_BITS_ALL;
        MemcpyUnrelatedSafe( vk_instance_data.transform, rhi_instance.transform );

        auto* rhi_blas = RHIImpl( rhi_instance.blas );
        if ( !SE_ENSURE( rhi_blas ) )
            continue;
        vk_instance_data.accelerationStructureReference = rhi_blas->GetVkASDeviceAddress();
    }

    m_gpu_buf->WriteBytes( vk_cpu_data.data(), vk_cpu_data.size() * sizeof( vk_cpu_data[0] ), 0 );
}

VkDeviceAddress VulkanASInstanceBuffer::GetVkDeviceAddress() const
{
    return RHIImpl( m_gpu_buf->GetBuffer() )->GetDeviceAddress();
}

void VulkanASInstanceBuffer::Init( size_t size )
{
    RHI::BufferInfo buf_info = {};
    buf_info.size = size;
    buf_info.usage = RHIBufferUsageFlags::AccelerationStructureInput;
    m_gpu_buf = RHIImpl( m_rhi->CreateUploadBuffer( buf_info ) );
}
