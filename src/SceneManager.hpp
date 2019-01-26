#pragma once

#include "SceneManager.h"

#include "FramegraphResource.h"

#include "ForwardCBProvider.h"

template<typename FramegraphT>
void SceneManager::BindToFramegraph( FramegraphT& framegraph, const ForwardCBProvider& forward_cb_provider )
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

		const auto& submesh_draw_args = submesh.DrawArgs();

		item.index_count = submesh_draw_args.idx_cnt;
		item.index_offset = submesh_draw_args.start_index_loc;
		item.vertex_offset = submesh_draw_args.base_vertex_loc;

		const MaterialPBR& material = m_scene.AllMaterials()[mesh_instance.Material()];
		item.mat_cb = material.GPUConstantBuffer();
		item.mat_table = material.DescriptorTable();

		bool has_unloaded_texture = false;
		const auto& textures = material.Textures();
		for ( TextureID tex_id : { textures.base_color, textures.normal, textures.specular } )
		{
			if ( ! m_scene.AllTextures()[tex_id].IsLoaded() )
			{
				has_unloaded_texture = true;
				break;
			}
		}

		if ( has_unloaded_texture )
			continue;

		const ObjectTransform& tf = m_scene.AllTransforms()[mesh_instance.GetTransform()];
		item.tf_addr = tf.GPUView();

		DirectX::BoundingOrientedBox item_box;
		DirectX::BoundingOrientedBox::CreateFromBoundingBox( item_box, submesh.Box() );

		item_box.Transform( item_box, DirectX::XMLoadFloat4x4( &tf.Obj2World() ) );

		if ( item_box.Intersects( main_bf ) )
			m_lighting_items.push_back( item );
	}

	ShadowProducers producers;
	ShadowMaps sm_storage;
	ShadowCascadeProducers pssm_producers;
	ShadowCascade pssm_storage;
	m_shadow_provider.FillFramegraphStructures( forward_cb_provider.GetLightsInCB(), m_scene.StaticMeshInstanceSpan(), producers, pssm_producers, sm_storage, pssm_storage );
	framegraph.SetRes( producers );
	framegraph.SetRes( sm_storage );
	framegraph.SetRes( pssm_producers );
	framegraph.SetRes( pssm_storage );

	MainRenderitems forward_renderitems;
	forward_renderitems.items = make_span( m_lighting_items );
	framegraph.SetRes( forward_renderitems );
}