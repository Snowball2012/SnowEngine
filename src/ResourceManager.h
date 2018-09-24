#pragma once

// Resource management
// Responsibilities:
// 1. Physical memory mapping for reserved resources
// 2. CPU -> GPU upload (Async / sync)
// 3. Resource availability tracking
// 4. Transient resource management (sets of non-overlapping transient resources -> sets of overlapping resources, placed optimally in a heap)
// 5. Async memory defragmentation

// This class does NOT manage resource views in any way.
// However, a resource is persistent after allocation,
// so you can safely create any descriptor you want and
// be sure it will stay valid until the resource is destroyed

// Transient resources must only be created and destroyed in bundles.

class ResourceManager
{
public:
	using ID = size_t;
	static constexpr UINT AllSubresources = 0xffffffff;

	void RemoveResource( ID resource_id );

	enum class AvailableOperations
	{
		ReadWrite,
		ReadOnly,
		None
	};

	// non-blocking
	AvailableOperations IsSubresourceAvailableGPU( ID resource_id, UINT subresource ) const;
	AvailableOperations IsSubresourceAvailableCPU( ID resource_id, UINT subresource ) const;

	// Does nothing if requested_operations == None
	// Notification may be executed in another thread
	using fnNotifyLoad = std::function<void( ID, int/*subresource*/, AvailableOperations )>;
	void NotifyOnResourceLoadCPU( ID resource_id, UINT subresource, AvailableOperations requested_operations, fnNotifyLoad callback );
	void NotifyOnResourceLoadGPU( ID resource_id, UINT subresource, AvailableOperations requested_operations, fnNotifyLoad callback );

	// return false if requested allocation is above current budget
	bool LoadSubresource( ID resource_id, UINT subresource, Microsoft::WRL::ComPtr<ID3D10Blob> data );
	void UnloadSubresource( ID resource_id, UINT subresource );

	// For resources in default heap available only if requested subresource is fully mapped to physical memory
	// and gpu-side subresources mentioned in the call will be unavailable for some time
	// Valid only if resource is available for CPU and GPU access, otherwise behaviour is undefined
	bool UpdateSubresource( ID resource_id, UINT subresource, void* data, size_t data_size );

	// Block the resource until all specified fence values are reached
	void BlockResourceGPU( ID resource_id, UINT subresource, UINT64 fence_val_copy, UINT64 fence_val_graphics, UINT64 fence_val_compute );
	void BlockResourceCPU( ID resource_id, UINT subresource, UINT64 fence_val_copy, UINT64 fence_val_graphics, UINT64 fence_val_compute );

	// Update fence values for all queues and execute corresponding notifications
	// If wait_for_notifications == false, notifications will be executed in another thread
	void UpdateTimeline( bool wait_for_notifications );

private:
	// resources with same id may exist in both maps if the resource in question has additional upload heap storage
	std::unordered_map<ID, Microsoft::WRL::ComPtr<ID3D12Resource>> m_gpu_resources;
	std::unordered_map<ID, Microsoft::WRL::ComPtr<ID3D12Resource>> m_cpu_resources;
};