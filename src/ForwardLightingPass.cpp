#include "stdafx.h"
#include "ForwardLightingPass.h"

#include "RenderUtils.h"

void ForwardLightingPass::Draw( const ForwardLightingContext& context, bool wireframe, ID3D12GraphicsCommandList& cmd_list )
{
	cmd_list.SetPipelineState( wireframe ? m_pso_wireframe : m_pso );
	
	cmd_list.OMSetRenderTargets( 1, context.back_buffer_rtv, true, context.depth_stencil_view );

	cmd_list.SetGraphicsRootSignature( m_root_signature );
	cmd_list.SetGraphicsRootConstantBufferView( 5, context.pass_cb->GetGPUVirtualAddress() );

	const auto obj_cb_adress = context.object_cb->GetGPUVirtualAddress();
	const auto obj_cb_size =  Utils::CalcConstantBufferByteSize( sizeof( ObjectConstants ) );
	for ( const auto& render_item : context.scene->renderitems )
	{
		cmd_list.SetGraphicsRootConstantBufferView( 0, obj_cb_adress + render_item.cb_idx * obj_cb_size );
		cmd_list.SetGraphicsRootConstantBufferView( 1, render_item.material->cb_gpu->GetGPUVirtualAddress() );
		cmd_list.SetGraphicsRootDescriptorTable( 2, render_item.material->base_color_desc );
		cmd_list.SetGraphicsRootDescriptorTable( 3, render_item.material->normal_map_desc );
		cmd_list.SetGraphicsRootDescriptorTable( 4, render_item.material->specular_desc );
		cmd_list.IASetVertexBuffers( 0, 1, &render_item.geom->VertexBufferView() );
		cmd_list.IASetIndexBuffer( &render_item.geom->IndexBufferView() );
		cmd_list.IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
		cmd_list.DrawIndexedInstanced( render_item.index_count, 1, render_item.index_offset, render_item.vertex_offset, 0 );
	}
}



