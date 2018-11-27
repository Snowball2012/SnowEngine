// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "stdafx.h"

#include "DescriptorTableBakery.h"


DescriptorTableBakery::DescriptorTableBakery( Microsoft::WRL::ComPtr<ID3D12Device> device, D3D12_DESCRIPTOR_HEAP_TYPE type,
											  size_t n_bufferized_frames, size_t num_descriptors_baseline )
	: m_gpu_heaps( n_bufferized_frames ), m_device( std::move( device ) )
{
	assert( n_bufferized_frames > 0 );
	assert( ( type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ) );
	assert( num_descriptors_baseline > 0 );	

	m_desc_size = m_device->GetDescriptorHandleIncrementSize( type );

	D3D12_DESCRIPTOR_HEAP_DESC desc{};
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.Type = type;
	desc.NumDescriptors = UINT( num_descriptors_baseline );
	ThrowIfFailed( m_device->CreateDescriptorHeap( &desc, IID_PPV_ARGS( m_staging_heap.heap.GetAddressOf() ) ) );

	desc.Flags |= D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	for ( auto& heap_instance : m_gpu_heaps )
		ThrowIfFailed( m_device->CreateDescriptorHeap( &desc, IID_PPV_ARGS( heap_instance.GetAddressOf() ) ) );
}


bool DescriptorTableBakery::BakeGPUTables()
{
	if ( m_gpu_tables_need_update )
	{
		m_cur_gpu_heap_idx = ( m_cur_gpu_heap_idx + 1 ) % m_gpu_heaps.size();

		auto& cur_gpu_heap = CurrentGPUHeap();
	
		if ( cur_gpu_heap->GetDesc().NumDescriptors < m_staging_heap.heap_end )
		{
			D3D12_DESCRIPTOR_HEAP_DESC gpu_heap_desc = m_staging_heap.heap->GetDesc();
			gpu_heap_desc.Flags |= D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			ThrowIfFailed( m_device->CreateDescriptorHeap( &gpu_heap_desc, IID_PPV_ARGS( cur_gpu_heap.GetAddressOf() ) ) );
		}

		m_device->CopyDescriptorsSimple( UINT( m_staging_heap.heap_end ),
										 cur_gpu_heap->GetCPUDescriptorHandleForHeapStart(),
										 m_staging_heap.heap->GetCPUDescriptorHandleForHeapStart(),
										 cur_gpu_heap->GetDesc().Type );

		m_gpu_tables_need_update = false;
		return true;
	}
	return false;
}


DescriptorTableBakery::TableID DescriptorTableBakery::AllocateTable( size_t ndescriptors )
{
	m_gpu_tables_need_update = true;

	constexpr float heap_expansion_rate = 1.5f;
	size_t new_end = ndescriptors + m_staging_heap.heap_end;
	if ( new_end > m_staging_heap.heap->GetDesc().NumDescriptors )
	{
		Rebuild( m_staging_heap, new_end * heap_expansion_rate );
		new_end = ndescriptors + m_staging_heap.heap_end;
		assert( new_end <= m_staging_heap.heap->GetDesc().NumDescriptors );
	}

	 TableID id = m_tables.insert( DescriptorTableData{ ndescriptors, m_staging_heap.heap_end } );
	 m_staging_heap.heap_end = new_end;
	 return id;
}


void DescriptorTableBakery::Reserve( size_t ndescriptors )
{
	if ( ndescriptors > m_staging_heap.heap->GetDesc().NumDescriptors )
		Rebuild( m_staging_heap, ndescriptors );
}


size_t DescriptorTableBakery::GetStagingHeapSize() const noexcept
{
	return m_staging_heap.heap_end;
}


void DescriptorTableBakery::EraseTable( TableID id ) noexcept
{
	m_tables.erase( id );
}


std::optional<DescriptorTableBakery::TableInfo> DescriptorTableBakery::GetTable( TableID id ) const noexcept
{
	if ( const DescriptorTableData* table = m_tables.try_get( id ) )
	{
		TableInfo retval;
		retval.ndescriptors = table->ndescriptors;
		retval.gpu_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE( CurrentGPUHeap()->GetGPUDescriptorHandleForHeapStart(), INT( m_desc_size * table->offset_in_descriptors ) );
		return std::make_optional( retval );
	}
	return std::nullopt;
}


std::optional<D3D12_CPU_DESCRIPTOR_HANDLE> DescriptorTableBakery::ModifyTable( TableID id ) noexcept
{
	if ( DescriptorTableData* table = m_tables.try_get( id ) )
	{
		m_gpu_tables_need_update = true;
		auto retval = std::optional<D3D12_CPU_DESCRIPTOR_HANDLE>();
		retval.emplace();

		CD3DX12_CPU_DESCRIPTOR_HANDLE::InitOffsetted( *retval,
													  m_staging_heap.heap->GetCPUDescriptorHandleForHeapStart(),
													  INT( m_desc_size * table->offset_in_descriptors ) );
		return retval;
	}
	return std::nullopt;
}


void DescriptorTableBakery::Rebuild( StagingHeap& staging_heap, size_t new_capacity )
{
	D3D12_DESCRIPTOR_HEAP_DESC new_heap_desc = staging_heap.heap->GetDesc();

	assert( new_capacity > new_heap_desc.NumDescriptors );
	new_heap_desc.NumDescriptors = UINT( new_capacity );

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> new_heap;
	ThrowIfFailed( m_device->CreateDescriptorHeap( &new_heap_desc, IID_PPV_ARGS( new_heap.GetAddressOf() ) ) );

	UINT new_end = 0;
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> src_ranges_start;
	src_ranges_start.reserve( m_tables.size() );
	std::vector<UINT> src_ranges_size;
	src_ranges_size.reserve( m_tables.size() );

	for ( DescriptorTableData& table_data : m_tables )
	{
		src_ranges_start.push_back( CD3DX12_CPU_DESCRIPTOR_HANDLE( staging_heap.heap->GetCPUDescriptorHandleForHeapStart(), INT( table_data.offset_in_descriptors * m_desc_size ) ) );
		src_ranges_size.push_back( UINT( table_data.ndescriptors ) );

		table_data.offset_in_descriptors = size_t( new_end );
		new_end += UINT( table_data.ndescriptors );
	}

	UINT num_descriptors_in_old_heap = new_end;
	
	m_device->CopyDescriptors( 1, &new_heap->GetCPUDescriptorHandleForHeapStart(), &num_descriptors_in_old_heap,
							   UINT( src_ranges_start.size() ), src_ranges_start.data(), src_ranges_size.data(),
							   new_heap_desc.Type );

	staging_heap.heap = std::move( new_heap );
	staging_heap.heap_end = size_t( new_end );
}


Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& DescriptorTableBakery::CurrentGPUHeap() noexcept
{
	return m_gpu_heaps[m_cur_gpu_heap_idx];
}


const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& DescriptorTableBakery::CurrentGPUHeap() const noexcept
{
	return m_gpu_heaps[m_cur_gpu_heap_idx];
}
