#pragma once

#include "PipelineResource.h"


template<class Pipeline>
class UIPassNode : public BaseRenderNode
{
public:
	using InputResources = std::tuple
		<
		TonemappedBackbuffer,
		ImGuiFontHeap
		>;

	using OutputResources = std::tuple
		<
		FinalBackbuffer
		>;

	UIPassNode( Pipeline* pipeline )
		: m_pipeline( pipeline )
	{}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override
	{
		TonemappedBackbuffer backbuffer;
		m_pipeline->GetRes( backbuffer );

		ImGuiFontHeap heap;
		m_pipeline->GetRes( heap );

		ImGui_ImplDX12_NewFrame( &cmd_list );
		cmd_list.SetDescriptorHeaps( 1, &heap.heap );
		ImGui_ImplDX12_RenderDrawData( ImGui::GetDrawData() );

		FinalBackbuffer out{ backbuffer.rtv };
		m_pipeline->SetRes( out );
	}

private:
	Pipeline* m_pipeline;
};

