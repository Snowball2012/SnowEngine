#include "stdafx.h"

#include "GPUDevice.h"

GPUResource::GPUResource(ComPtr<ID3D12Resource> native_resource, IGPUResourceDeleter* deleter)
    : m_native_resource(native_resource.Get())
{
    if (deleter)
    {
        switch (deleter->GetRegisterTime())
        {
            case IGPUResourceDeleter::RegisterTime::OnCreate:
            {
                deleter->RegisterResource(*this);
                break;
            }
            case IGPUResourceDeleter::RegisterTime::OnRelease:
            {
                m_native_resource_holder = std::move(native_resource);
                m_deleter = deleter;
                break;
            }
            default:
                NOTIMPL;
        }
    }
}

GPUResource::GPUResource(GPUResource&& other)
    : m_native_resource(other.m_native_resource)
    , m_native_resource_holder(std::move(other.m_native_resource_holder))
    , m_deleter(other.m_deleter)
{
    other.m_deleter = nullptr;
    other.m_native_resource_holder = nullptr;
    other.m_native_resource = nullptr;
}

GPUResource& GPUResource::operator=(GPUResource&& other)
{
    m_native_resource = other.m_native_resource;
    m_native_resource_holder = std::move(other.m_native_resource_holder);
    m_deleter = other.m_deleter;
    
    other.m_deleter = nullptr;
    other.m_native_resource_holder = nullptr;
    other.m_native_resource = nullptr;

    return *this;
}

GPUResource::~GPUResource()
{
    if (m_deleter)
        m_deleter->RegisterResource(*this);
}

GPUDevice::GPUDevice(ComPtr<ID3D12Device> native_device)
    : m_d3d_device(std::move(native_device))
{
    SE_ENSURE(m_d3d_device.Get());
    m_descriptor_size.rtv = m_d3d_device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );
    m_descriptor_size.dsv = m_d3d_device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_DSV );
    m_descriptor_size.cbv_srv_uav = m_d3d_device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
}

GPUResource GPUDevice::CreateResource(IGPUResourceDeleter* deleter)
{
    NOTIMPL;
    if (!deleter)
        deleter = this;
    
    return std::move(GPUResource(nullptr, deleter));
}

void GPUDevice::RegisterResource(GPUResource& resource)
{
    NOTIMPL;
}
