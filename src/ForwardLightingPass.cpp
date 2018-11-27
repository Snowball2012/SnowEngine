// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"
#include "ForwardLightingPass.h"

#include "RenderUtils.h"

ForwardLightingPass::ForwardLightingPass( ID3D12PipelineState* pso, ID3D12PipelineState* wireframe_pso, ID3D12RootSignature* rootsig )
	: m_pso( pso ), m_pso_wireframe( wireframe_pso ), m_root_signature( rootsig ) {}

void ForwardLightingPass::Draw( const Context& context, bool wireframe, ID3D12GraphicsCommandList& cmd_list )
{
	cmd_list.SetPipelineState( wireframe ? m_pso_wireframe : m_pso );
	
	D3D12_CPU_DESCRIPTOR_HANDLE render_targets[3] =
	{
		context.back_buffer_rtv,
		context.ambient_rtv,
		context.normals_rtv
	};

	cmd_list.OMSetRenderTargets( 3, render_targets, false, &context.depth_stencil_view );

	cmd_list.SetGraphicsRootSignature( m_root_signature );

	cmd_list.SetGraphicsRootConstantBufferView( 4, context.pass_cb );

	cmd_list.SetGraphicsRootDescriptorTable( 3, context.shadow_map_srv );

	for ( const auto& render_item : *context.renderitems )
	{
		cmd_list.SetGraphicsRootConstantBufferView( 0, render_item.tf_addr );
		cmd_list.SetGraphicsRootConstantBufferView( 1, render_item.mat_cb );
		cmd_list.SetGraphicsRootDescriptorTable( 2, render_item.mat_table );
		cmd_list.IASetVertexBuffers( 0, 1, &render_item.vbv );
		cmd_list.IASetIndexBuffer( &render_item.ibv );
		cmd_list.IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
		cmd_list.DrawIndexedInstanced( render_item.index_count, 1, render_item.index_offset, render_item.vertex_offset, 0 );
	}
}

ForwardLightingPass::Shaders ForwardLightingPass::LoadAndCompileShaders()
{
	return Shaders
	{
		Utils::LoadBinary( L"shaders/vs.cso" ),
		Utils::LoadBinary( L"shaders/gs.cso" ),
		Utils::LoadBinary( L"shaders/ps.cso" )
	};
}