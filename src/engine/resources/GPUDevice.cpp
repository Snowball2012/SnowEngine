#include "stdafx.h"

#include "GPUDevice.h"

GPUSharedResource::~GPUSharedResource()
{
    if (SE_ENSURE(m_owner))
        m_owner->ReleaseSharedResource(*this);        
}

GPUSharedResourcePtr GPUDevice::CreateSharedResource()
{
    NOTIMPL;
    ISharedResourceDeleter* deleter_iface = this;
    return std::make_shared<GPUSharedResource>(nullptr, deleter_iface);
}

GPUFrameResourcePtr GPUDevice::CreateFrameResource(GPUResourceHolder& frame_resources)
{
    NOTIMPL;
    return nullptr;
}

void GPUDevice::ReleaseSharedResource(GPUResource& resource)
{
    NOTIMPL;
}
