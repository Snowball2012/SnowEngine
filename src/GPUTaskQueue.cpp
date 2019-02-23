// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"
#include "GPUTaskQueue.h"


GPUTaskQueue::GPUTaskQueue( ID3D12Device& device, D3D12_COMMAND_LIST_TYPE type, std::shared_ptr<CommandListPool> pool )
	: m_type( type ), m_pool( std::move( pool ) )
{
	ThrowIfFailed( device.CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( &m_fence ) ) );
	D3D12_COMMAND_QUEUE_DESC queue_desc = {};
	queue_desc.Type = type;
	queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed( device.CreateCommandQueue( &queue_desc, IID_PPV_ARGS( &m_cmd_queue ) ) );
}


GPUTaskQueue::~GPUTaskQueue()
{
	// Sadly, we must catch possible exceptions here since Flush may throw some, and we don't want to crash the program because of that
	try
	{
		Flush();
	}
	catch ( const Utils::DxException& )
	{
		// do nothing
		// TODO: log error?
	}
}


ID3D12CommandQueue* GPUTaskQueue::GetCmdQueue() noexcept
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


void GPUTaskQueue::SubmitLists( const span<CommandList>& lists ) noexcept
{
	for ( CommandList& list : lists )
	{
		m_submitted_interfaces.push_back( list.GetInterface() );
		m_submitted_lists.emplace_back( std::move( list ) );
	}
}


GPUTaskQueue::Timestamp GPUTaskQueue::ExecuteSubmitted()
{
	m_cmd_queue->ExecuteCommandLists( UINT( m_submitted_interfaces.size() ), m_submitted_interfaces.data() );
	m_submitted_interfaces.clear();

	Timestamp finish_time = CreateTimestamp();

	for ( CommandList& list : m_submitted_lists )
		m_scheduled_lists.push( ScheduledList{ std::move( list ), finish_time } );

	m_submitted_lists.clear();

	return finish_time;
}


GPUTaskQueue::Timestamp GPUTaskQueue::GetCurrentTimestamp()
{
	return m_fence->GetCompletedValue();
}


void GPUTaskQueue::WaitForTimestamp( Timestamp ts )
{
	if ( GetCurrentTimestamp() < ts )
	{
		HANDLE eventHandle = CreateEventEx( nullptr, nullptr, 0, EVENT_ALL_ACCESS );

		// Fire event when GPU hits current fence.  
		ThrowIfFailed( m_fence->SetEventOnCompletion( ts, eventHandle ) );

		// Wait until the GPU hits current fence event is fired.
		WaitForSingleObject( eventHandle, INFINITE );
		CloseHandle( eventHandle );
	}

	ReleaseProcessedLists( ts );
}


void GPUTaskQueue::Flush()
{
	WaitForTimestamp( CreateTimestamp() );
}


void GPUTaskQueue::ReleaseProcessedLists( Timestamp ts )
{
	while ( !m_scheduled_lists.empty() )
	{
		auto& scheduled_list = m_scheduled_lists.front();
		if ( scheduled_list.finish_time > ts )
			break;

		m_pool->PutList( std::move( scheduled_list.list ) );
		m_scheduled_lists.pop();
	}
}
