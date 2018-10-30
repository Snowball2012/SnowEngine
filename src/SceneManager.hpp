#pragma once

#include "SceneManager.h"

#include "PipelineResource.h"

template<typename PipelineT>
void SceneManager::BindToPipeline( PipelineT& pipeline,
								   DirectX::XMFLOAT4X4 camera_frustrum_proj, DirectX::XMFLOAT4X4 camera_frustrum_view,
								   DirectX::XMFLOAT4X4 sun_frustrum_proj, DirectX::XMFLOAT4X4 sun_frustrum_view )
{
	m_lighting_items.clear();
	m_shadow_items.clear();

	DirectX::BoundingFrustum main_bf( DirectX::XMLoadFloat4x4( &camera_frustrum_proj ) );
	DirectX::XMVECTOR det;
	main_bf.Transform( main_bf, DirectX::XMMatrixInverse( &det, DirectX::XMLoadFloat4x4( &camera_frustrum_view ) ) );

	//DirectX::BoundingOrientedBox shadow_bb;
	DirectX::BoundingFrustum shadow_bf( DirectX::XMLoadFloat4x4( &sun_frustrum_proj ) );
	shadow_bf.Transform( shadow_bf, DirectX::XMMatrixInverse( &det, DirectX::XMLoadFloat4x4( &sun_frustrum_view ) ) );
	for ( const auto& mesh_instance : m_scene.StaticMeshInstanceSpan() )
	{
		const StaticSubmesh& submesh = m_scene.AllStaticSubmeshes()[mesh_instance.Submesh()];
		const StaticMesh& geom = m_scene.AllStaticMeshes()[submesh.GetMesh()];
		if ( ! geom.IsLoaded() )
			continue;

		RenderItem item;
		item.ibv = geom.IndexBufferView();
		item.vbv = geom.VertexBufferView();
		item.index_count = submesh.DrawArgs().idx_cnt;
		item.index_offset = submesh.DrawArgs().start_index_loc;
		item.vertex_offset = submesh.DrawArgs().base_vertex_loc;

		const MaterialPBR& material = m_scene.AllMaterials()[mesh_instance.Material()];
		item.mat_cb = material.GPUConstantBuffer();
		item.mat_table = material.DescriptorTable();

		bool has_unloaded_texture = false;
		for ( TextureID tex_id : { material.Textures().base_color, material.Textures().normal, material.Textures().specular } )
		{
			if ( ! m_scene.AllTextures()[tex_id].IsLoaded() )
			{
				has_unloaded_texture = true;
				break;
			}
		}

		if ( has_unloaded_texture )
			continue;

		const ObjectTransform& tf = m_scene.AllTransforms()[mesh_instance.Transform()];
		item.tf_addr = tf.GPUView();

		DirectX::BoundingBox item_box;
		submesh.Box().Transform( item_box, DirectX::XMLoadFloat4x4( &tf.Obj2World() ) );

		if ( item_box.Intersects( main_bf ) )
			m_lighting_items.push_back( item );
		//if ( item_box.Intersects( shadow_bf ) )
			m_shadow_items.push_back( item );
	}

	ShadowCasters sc;
	sc.casters = &m_shadow_items;
	pipeline.SetRes( sc );

	SceneContext forward_renderitems;
	forward_renderitems.items = &m_lighting_items;
	pipeline.SetRes( forward_renderitems );
}