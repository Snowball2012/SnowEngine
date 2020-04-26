#pragma once

#include <wrl.h>
#include <d3d12.h>

#include <unordered_set>

class DescriptorHeap;

// invalid without DescriptorHeap that created it
class Descriptor
{
public:
    ~Descriptor();

    Descriptor( const Descriptor& desc ) = delete;
    Descriptor& operator=( const Descriptor& desc ) = delete;

    Descriptor( Descriptor&& desc );
    Descriptor& operator=( Descriptor&& desc );


    const D3D12_CPU_DESCRIPTOR_HANDLE HandleCPU() const { return m_cpu_handle; }
    const D3D12_GPU_DESCRIPTOR_HANDLE HandleGPU() const { return m_gpu_handle; }

private:
    friend class DescriptorHeap;
    Descriptor( DescriptorHeap* heap, int offset );

    void Reset() { m_offset = -1; m_heap = nullptr; }

    D3D12_CPU_DESCRIPTOR_HANDLE m_cpu_handle;
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpu_handle;
    DescriptorHeap* m_heap = nullptr;
    int m_offset = -1;
};

// simple descriptor allocator, contigious allocations are not supported yet
class DescriptorHeap
{
public:
    DescriptorHeap( Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> desc_heap_iface, size_t descriptor_size_in_bytes );

    // no copying allowed, only moving
    DescriptorHeap( const DescriptorHeap& ) = delete;
    DescriptorHeap& operator=( const DescriptorHeap& ) = delete;
    DescriptorHeap( DescriptorHeap&& ) = default;
    DescriptorHeap& operator=( DescriptorHeap&& ) = default;

    size_t GetSizeInDescriptors() const { return m_desc_size; }
    size_t GetSpareDescriptorsCount() const { return m_spare_descriptor_cnt; }
    size_t GetTotalDescriptorsCount() const { return m_desc_total; }

    // can't be constant by design
    ID3D12DescriptorHeap* GetInterface() { return m_heap.Get(); }
    
    Descriptor AllocateDescriptor();

private:
    friend class Descriptor;
    void ReleaseDescriptor( int offset );

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_heap = nullptr;
    size_t m_desc_size = 0;
    size_t m_spare_descriptor_cnt = 0;
    int m_free_segment_start = 0;
    size_t m_desc_total = 0;
    std::unordered_set<int> m_recycled_descriptors;
};