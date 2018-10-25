#pragma once

#include <wrl.h>
#include <d3d12.h>
#include "utils/packed_freelist.h"

// Bakes multiple tables into single GPU-visible descriptor heap
class DescriptorTableBakery
{
	struct DescriptorTableData
	{
		size_t ndescriptors;
		size_t offset_in_descriptors;
	};

public:
	using TableID = packed_freelist<DescriptorTableData>::id;

	DescriptorTableBakery( Microsoft::WRL::ComPtr<ID3D12Device> device, D3D12_DESCRIPTOR_HEAP_TYPE type,
						   size_t n_bufferized_frames, size_t num_descriptors_baseline = 100 );

	// Fills gpu_handles for every descriptor table, returns true if tables have been changed
	bool BakeGPUTables();
	const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& CurrentGPUHeap() const noexcept;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& CurrentGPUHeap() noexcept;

	// Can potentially call ID3D12Device::CopyDescriptors() and defragment the whole table, Reserve() before adding multiple tables when possible
	TableID AllocateTable( size_t ndescriptors );
	void Reserve( size_t ndescriptors );
	size_t GetStagingHeapSize() const noexcept;
	void EraseTable( TableID id ) noexcept;

	// gpu_handle is valid only for one frame
	struct TableInfo
	{
		D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle;
		size_t ndescriptors;
	};
	std::optional<TableInfo> GetTable( TableID id ) const noexcept;

	// returned pointer may be invalidated after subsequent AllocateTable call. Returns nullptr if there is no table does not exist
	std::optional<D3D12_CPU_DESCRIPTOR_HANDLE> ModifyTable( TableID id ) noexcept;

private:

	struct StagingHeap
	{
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
		size_t heap_end = 0;
	};

	void Rebuild( StagingHeap& heap, size_t new_capacity );

	packed_freelist<DescriptorTableData> m_tables;

	// we can add new tables to the end of the heap as long as heap->GetDesc().NumDescriptors >= heap_end + table.ndescriptors
	StagingHeap m_staging_heap;

	std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> m_gpu_heaps;
	size_t m_cur_gpu_heap_idx = 0;

	size_t m_desc_size;

	bool m_gpu_tables_need_update = false;

	Microsoft::WRL::ComPtr<ID3D12Device> m_device;
};