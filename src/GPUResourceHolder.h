#pragma once

#include "utils/span.h"
#include "GPUAllocators.h"

// This class holds all resources created by a frame until the frame is fully processed
// It respects dependencies between various types of objects (e.g. resources will be freed 
//     before allocators, descriptors before resources, etc.)
class GPUResourceHolder
{
public:
    GPUResourceHolder( const GPUResourceHolder& ) = delete;
    GPUResourceHolder( GPUResourceHolder&& ) = default;

    ~GPUResourceHolder();

    void Merge( GPUResourceHolder&& other );

    void AddAllocators( const span<GPULinearAllocator>& allocators );
    void AddResources( const span<ComPtr<ID3D12Resource>>& resources );

private:
    std::vector<GPULinearAllocator> m_allocators;
    std::vector<ComPtr<ID3D12Resource>> m_resources;
};