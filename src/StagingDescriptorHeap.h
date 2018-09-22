#pragma once

#include "DescriptorHeap.h"

// Dynamic descriptor heap of variable size
// Actually consists of several hardware descriptor heaps, so it should be used only for CPU access
// Descriptors are automatically released when out of scope
class StagingDescriptorHeap
{
public:
	StagingDescriptorHeap( D3D12_DESCRIPTOR_HEAP_TYPE type, Microsoft::WRL::ComPtr<ID3D12Device> device );
	Descriptor AllocateDescriptor();

private:
	static constexpr size_t DefaultChunkSizeInDescriptors = 1024;

	D3D12_DESCRIPTOR_HEAP_TYPE m_type = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;

	void AddChunk();

	std::list<DescriptorHeap> m_chunks; // all DescriptorHeaps must remain valid once allocated so all descriptors can be safely deallocated

	Microsoft::WRL::ComPtr<ID3D12Device> m_device;
};