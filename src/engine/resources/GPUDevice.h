#pragma once

#include "../Ptr.h"

#include <d3d12.h>

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

    GPUDescriptorSizes m_descriptor_size;
    
public:
    GPUDevice(ComPtr<ID3D12Device> native_device);
    
    ID3D12Device* GetNativeDevice() const { return m_d3d_device.Get(); }

    GPUResource CreateResource(IGPUResourceDeleter* deleter = nullptr);

    const GPUDescriptorSizes& GetDescriptorSizes() const { return m_descriptor_size; }

private:
    virtual void RegisterResource(GPUResource& resource) override;
    virtual RegisterTime GetRegisterTime() const override { return RegisterTime::OnRelease; };
};