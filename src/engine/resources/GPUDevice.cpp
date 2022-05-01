#include "stdafx.h"

#include "GPUDevice.h"

#include "engine/GPUTaskQueue.h"

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

GPUDevice::GPUDevice(ComPtr<ID3D12Device> native_device, bool enable_raytracing)
    : m_d3d_device(std::move(native_device))
{
    SE_ENSURE(m_d3d_device.Get());
    m_descriptor_size.rtv = m_d3d_device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );
    m_descriptor_size.dsv = m_d3d_device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_DSV );
    m_descriptor_size.cbv_srv_uav = m_d3d_device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
    
    m_cmd_lists = std::make_shared<CommandListPool>( m_d3d_device.Get() );
    
    m_graphics_queue = std::make_unique<GPUTaskQueue>( *m_d3d_device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT, m_cmd_lists );
    m_copy_queue = std::make_unique<GPUTaskQueue>( *m_d3d_device.Get(), D3D12_COMMAND_LIST_TYPE_COPY, m_cmd_lists );
    m_compute_queue = std::make_unique<GPUTaskQueue>( *m_d3d_device.Get(), D3D12_COMMAND_LIST_TYPE_COMPUTE, m_cmd_lists );

    if (enable_raytracing)
        ThrowIfFailedH(m_d3d_device->QueryInterface(m_rt_device.GetAddressOf()));
}

GPUTaskQueue* GPUDevice::GetGraphicsQueue()
{
    return m_graphics_queue.get();
}

GPUTaskQueue* GPUDevice::GetComputeQueue()
{
    return m_compute_queue.get();
}

GPUTaskQueue* GPUDevice::GetCopyQueue()
{
    return m_copy_queue.get();
}

void GPUDevice::Flush()
{
    m_copy_queue->Flush();
    m_compute_queue->Flush();
    m_graphics_queue->Flush();
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
