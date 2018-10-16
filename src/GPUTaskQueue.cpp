#include "stdafx.h"
#include "GPUTaskQueue.h"

GPUTaskQueue::GPUTaskQueue( ID3D12Device& device, D3D12_COMMAND_LIST_TYPE type )
{
	ThrowIfFailed( device.CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( &m_fence ) ) );
	D3D12_COMMAND_QUEUE_DESC queue_desc = {};
	queue_desc.Type = type;
	queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed( device.CreateCommandQueue( &queue_desc, IID_PPV_ARGS( &m_cmd_queue ) ) );
}

ID3D12CommandQueue* GPUTaskQueue::GetCmdQueue()
{
	return m_cmd_queue.Get();
}

GPUTaskQueue::Timestamp GPUTaskQueue::CreateTimestamp()
{
	m_current_fence_value++;
	ThrowIfFailed( m_cmd_queue->Signal( m_fence.Get(), m_current_fence_value ) );
	return m_current_fence_value;
}

GPUTaskQueue::Timestamp GPUTaskQueue::GetLastPlacedTimestamp() const
{
	return m_current_fence_value;
}

GPUTaskQueue::Timestamp GPUTaskQueue::GetCurrentTimestamp()
{
	return m_fence->GetCompletedValue();
}

void GPUTaskQueue::WaitForTimestamp( Timestamp ts )
{
	if ( GetCurrentTimestamp() < ts )
	{
		HANDLE eventHandle = CreateEventEx( nullptr, false, false, EVENT_ALL_ACCESS );

		// Fire event when GPU hits current fence.  
		ThrowIfFailed( m_fence->SetEventOnCompletion( ts, eventHandle ) );

		// Wait until the GPU hits current fence event is fired.
		WaitForSingleObject( eventHandle, INFINITE );
		CloseHandle( eventHandle );
	}
}

void GPUTaskQueue::Flush()
{
	WaitForTimestamp( CreateTimestamp() );
}
