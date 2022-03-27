#pragma once

#include "../Ptr.h"

#include <d3d12.h>

class GPUDevice;

class GPUResource
{
private:
    ComPtr<ID3D12Resource> m_native_resource = nullptr;
    
public:
    GPUResource() = delete;
    GPUResource(ComPtr<ID3D12Resource> native_resource) : m_native_resource(native_resource) {}
    virtual ~GPUResource() {}
    ID3D12Resource* operator->() { return m_native_resource.Get(); }
    const ID3D12Resource* operator->() const { return m_native_resource.Get(); }
};

class ISharedResourceDeleter
{
public:
    virtual ~ISharedResourceDeleter() {}
    
    virtual void ReleaseSharedResource(GPUResource& resource) = 0;
};

class GPUSharedResource : public GPUResource
{
private:
    ISharedResourceDeleter* m_owner = nullptr;
    
public:
    virtual ~GPUSharedResource() override;
    GPUSharedResource(ComPtr<ID3D12Resource> native_resource, ISharedResourceDeleter* owner) : GPUResource(native_resource.Get()), m_owner(owner)
    {}
};

using GPUSharedResourcePtr = std::shared_ptr<GPUSharedResource>;

class GPUFrameResource : public GPUResource
{
private:
    GPUFrameResource(ComPtr<ID3D12Resource> native_resource) : GPUResource(native_resource) {}
};

using GPUFrameResourcePtr = GPUResource*;

class GPUDevice : private ISharedResourceDeleter
{
private:
    ComPtr<ID3D12Device> m_d3d_device;

    friend class GPUFrameResource;
    
public:
    ID3D12Device* GetNativeDevice() const { return m_d3d_device.Get(); }

    GPUSharedResourcePtr CreateSharedResource();
    GPUFrameResourcePtr CreateFrameResource(class GPUResourceHolder& frame_resources);

private:
    virtual void ReleaseSharedResource(GPUResource& resource) override;
};