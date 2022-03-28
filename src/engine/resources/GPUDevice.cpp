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
