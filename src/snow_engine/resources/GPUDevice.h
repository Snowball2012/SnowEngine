﻿#pragma once

#include "../Ptr.h"

#include <d3d12.h>

class GPUTaskQueue;
class CommandListPool;
class GPUDevice;

class IGPUResourceDeleter;

class GPUResource
{
private:
    ID3D12Resource* m_native_resource = nullptr;
    ComPtr<ID3D12Resource> m_native_resource_holder = nullptr;
    IGPUResourceDeleter* m_deleter = nullptr;
    
    friend class GPUDevice;
    
    GPUResource(ComPtr<ID3D12Resource> native_resource, IGPUResourceDeleter* deleter);

public:
    ~GPUResource();
    
    GPUResource() = delete;
    
    GPUResource(GPUResource&& other);
    GPUResource& operator=(GPUResource&& other);
    
    GPUResource(const GPUResource&) = delete;
    GPUResource& operator=(const GPUResource&) = delete;
    
    ID3D12Resource* operator->() { return m_native_resource; }
    const ID3D12Resource* operator->() const { return m_native_resource; }
};

class IGPUResourceDeleter
{
public:
    virtual ~IGPUResourceDeleter() {}

    virtual void RegisterResource(GPUResource& resource) = 0;

    enum class RegisterTime
    {
        OnCreate,
        OnRelease
    };

    virtual RegisterTime GetRegisterTime() const = 0;
};

class GPUDevice : private IGPUResourceDeleter
{
public:
    struct GPUDescriptorSizes
    {
        size_t rtv = 0;
        size_t dsv = 0;
        size_t cbv_srv_uav = 0;
    };
    
private:
    ComPtr<ID3D12Device> m_d3d_device = nullptr;
    ComPtr<ID3D12Device5> m_rt_device = nullptr;

    GPUDescriptorSizes m_descriptor_size;

    std::shared_ptr<CommandListPool> m_cmd_lists = nullptr;

    std::unique_ptr<GPUTaskQueue> m_graphics_queue = nullptr;
    std::unique_ptr<GPUTaskQueue> m_copy_queue = nullptr;
    std::unique_ptr<GPUTaskQueue> m_compute_queue = nullptr;
    
public:
    GPUDevice(ComPtr<ID3D12Device> native_device, bool enable_raytracing);
    
    ID3D12Device* GetNativeDevice() { return m_d3d_device.Get(); }
    const ID3D12Device* GetNativeDevice() const  { return m_d3d_device.Get(); }
    
    ID3D12Device5* GetNativeRTDevice() { return m_rt_device.Get(); }
    const ID3D12Device5* GetNativeRTDevice() const  { return m_rt_device.Get(); }
    
    GPUTaskQueue* GetGraphicsQueue();
    GPUTaskQueue* GetComputeQueue();
    GPUTaskQueue* GetCopyQueue();

    bool IsRaytracingEnabled() const { return m_rt_device; }

    // Flush all queues
    void Flush();

    GPUResource CreateResource(IGPUResourceDeleter* deleter = nullptr);

    const GPUDescriptorSizes& GetDescriptorSizes() const { return m_descriptor_size; }

private:
    virtual void RegisterResource(GPUResource& resource) override;
    virtual RegisterTime GetRegisterTime() const override { return RegisterTime::OnRelease; };
};