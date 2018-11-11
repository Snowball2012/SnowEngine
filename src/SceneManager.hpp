#pragma once

#include "SceneManager.h"

#include "PipelineResource.h"

template<typename PipelineT>
void SceneManager::BindToPipeline( PipelineT& pipeline )
{
	const Camera* main_cam = m_scene.AllCameras().try_get( m_main_camera_id );
	if ( ! main_cam )
		throw SnowEngineException( "main camera not found" );
	const auto& cam_data = main_cam->GetData();
	if ( cam_data.type != Camera::Type::Perspective )
		NOTIMPL;

	DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH( cam_data.fov_y,
																cam_data.aspect_ratio,
																cam_data.near_plane,
																cam_data.far_plane );

	DirectX::BoundingFrustum main_bf( proj );

	DirectX::XMMATRIX view = DirectX::XMMatrixLookToLH( DirectX::XMLoadFloat3( &cam_data.pos ),
														DirectX::XMLoadFloat3( &cam_data.dir ),
														DirectX::XMLoadFloat3( &cam_data.up ) );
	DirectX::XMVECTOR det;
	main_bf.Transform( main_bf, DirectX::XMMatrixInverse( &det, view ) );

	m_lighting_items.clear();
	for ( const auto& mesh_instance : m_scene.StaticMeshInstanceSpan() )
	{
		if ( ! mesh_instance.IsEnabled() )
			continue;

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
	}

	ShadowProducers producers;
	ShadowMapStorage sm_storage;
	m_shadow_provider.FillPipelineStructures( m_scene.StaticMeshInstanceSpan(), producers, sm_storage );
	pipeline.SetRes( producers );
	pipeline.SetRes( sm_storage );

	MainRenderitems forward_renderitems;
	forward_renderitems.items = &m_lighting_items;
	pipeline.SetRes( forward_renderitems );
}