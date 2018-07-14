#include "stdafx.h"

#include "DescriptorHeap.h"

#include <d3dx12.h>

DescriptorHeap::DescriptorHeap( Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> desc_heap_iface, size_t descriptor_size_in_bytes )
	: m_heap( std::move( desc_heap_iface ) ), m_desc_size( descriptor_size_in_bytes )
{
	const auto& descriptor_heap_desc = m_heap->GetDesc();
	m_desc_total = descriptor_heap_desc.NumDescriptors;
	m_spare_descriptor_cnt = m_desc_total;
}

Descriptor DescriptorHeap::AllocateDescriptor()
{
	int offset = 0;
	if ( ! m_recycled_descriptors.empty() )
	{
		auto it = m_recycled_descriptors.begin();
		offset = *it;
		m_recycled_descriptors.erase( it );
	}
	else if ( m_free_segment_start < m_desc_total )
	{
		offset = m_free_segment_start++;
	}
	else
	{
		throw SnowEngineException( "no more descriptors in heap" );
	}
	m_spare_descriptor_cnt--;
	return std::move( Descriptor( this, offset ) );
}

void DescriptorHeap::ReleaseDescriptor( int offset )
{
	// does not really destroy the descriptor, just marks it's memory as free
	if ( m_recycled_descriptors.emplace( offset ).second )
		m_spare_descriptor_cnt++;
}

Descriptor::Descriptor( DescriptorHeap* heap, int offset )
	: m_heap( heap ), m_offset( offset )
{
	m_cpu_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE( m_heap->m_heap->GetCPUDescriptorHandleForHeapStart(), offset * INT( m_heap->m_desc_size ) );
	m_gpu_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE( m_heap->m_heap->GetGPUDescriptorHandleForHeapStart(), offset * INT( m_heap->m_desc_size ) );
}

Descriptor::~Descriptor()
{
	if ( m_heap && m_offset >= 0 )
		m_heap->ReleaseDescriptor( m_offset );
}

Descriptor::Descriptor( Descriptor&& desc )
{
	m_heap = desc.m_heap;
	m_offset = desc.m_offset;
	m_cpu_handle = desc.m_cpu_handle;
	m_gpu_handle = desc.m_gpu_handle;

	desc.Reset();
}

Descriptor& Descriptor::operator=( Descriptor&& desc )
{
	m_heap = desc.m_heap;
	m_offset = desc.m_offset;
	m_cpu_handle = desc.m_cpu_handle;
	m_gpu_handle = desc.m_gpu_handle;

	desc.Reset();
	return *this;
}
