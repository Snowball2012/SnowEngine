#pragma once

#include "PipelineResource.h"


template<class Pipeline>
class UIPassNode : public BaseRenderNode
{
public:
	using OpenRes = std::tuple
		<
		>;
	using WriteRes = std::tuple
		<
		>;
	using ReadRes = std::tuple
		<
		ImGuiFontHeap
		>;
	using CloseRes = std::tuple
		<
		SDRBuffer
		>;

	UIPassNode( Pipeline* pipeline )
		: m_pipeline( pipeline )
	{}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override
	{
		auto& backbuffer = m_pipeline->GetRes<SDRBuffer>();
		if ( ! backbuffer )
			throw SnowEngineException( "missing resource" );

		auto& heap = m_pipeline->GetRes<ImGuiFontHeap>();
		if ( ! heap )
			throw SnowEngineException( "missing resource" );

		ImGui_ImplDX12_NewFrame( &cmd_list );
		cmd_list.SetDescriptorHeaps( 1, &heap->heap );
		ImGui_ImplDX12_RenderDrawData( ImGui::GetDrawData() );
	}

private:
	Pipeline* m_pipeline;
};

