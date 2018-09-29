#pragma once

#include <d3d12.h>
#include <wrl.h>

class GPUTaskQueue
{
public:
	using Timestamp = UINT64;

	GPUTaskQueue( ID3D12Device& device, D3D12_COMMAND_LIST_TYPE type );

	ID3D12CommandQueue* GetCmdQueue();

	Timestamp CreateTimestamp();
	Timestamp GetLastPlacedTimestamp() const;

	// not const since 2 consequential calls may return different values
	Timestamp GetCurrentTimestamp();

	void WaitForTimestamp( Timestamp ts );

	// convinient equivalent of WaitForTimestamp( CreateTimestamp() )
	void Flush();

private:

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_cmd_queue = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Fence> m_fence = nullptr;
	UINT64 m_current_fence_value = 0;
};