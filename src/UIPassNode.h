#pragma once

#include "FramegraphResource.h"


template<class Framegraph>
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

	UIPassNode( Framegraph* framegraph )
		: m_framegraph( framegraph )
	{}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override
	{
		auto& backbuffer = m_framegraph->GetRes<SDRBuffer>();
		if ( ! backbuffer )
			throw SnowEngineException( "missing resource" );

		auto& heap = m_framegraph->GetRes<ImGuiFontHeap>();
		if ( ! heap )
			throw SnowEngineException( "missing resource" );

		ImGui_ImplDX12_NewFrame( &cmd_list );
		cmd_list.SetDescriptorHeaps( 1, &heap->heap );
		ImGui_ImplDX12_RenderDrawData( ImGui::GetDrawData() );
	}

private:
	Framegraph* m_framegraph;
};

