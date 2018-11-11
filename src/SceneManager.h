#pragma once

#include "RenderData.h"
#include "Scene.h"

#include "StaticMeshManager.h"
#include "TextureStreamer.h"
#include "DynamicSceneBuffers.h"
#include "DescriptorTableBakery.h"
#include "MaterialTableBaker.h"
#include "ShadowProvider.h"

#include <DirectXMath.h>
class SceneManager;
class GPUTaskQueue;

// update, remove and add methods for engine client
class SceneClientView
{
public:
	SceneClientView( Scene* scene,
					 StaticMeshManager* smm,
					 TextureStreamer* tex_streamer,
					 DynamicSceneBuffers* dynamic_buffers,
					 MaterialTableBaker* material_table_baker )
		: m_scene( scene )
		, m_static_mesh_manager( smm )
		, m_tex_streamer( tex_streamer )
		, m_dynamic_buffers( dynamic_buffers )
		, m_material_table_baker( material_table_baker ){}

	const Scene& GetROScene() const noexcept { return *m_scene; }

	StaticMeshID LoadStaticMesh( std::string name, std::vector<Vertex> vertices, std::vector<uint32_t> indices );
	TextureID LoadStreamedTexture( std::string path );
	TransformID AddTransform( DirectX::XMFLOAT4X4 obj2world = Identity4x4 );
	MaterialID AddMaterial( const MaterialPBR::TextureIds& textures, DirectX::XMFLOAT3 diffuse_fresnel, DirectX::XMFLOAT4X4 uv_transform = Identity4x4 );
	StaticSubmeshID AddSubmesh( StaticMeshID mesh_id, const StaticSubmesh::Data& data );
	MeshInstanceID AddMeshInstance( StaticSubmeshID submesh_id, TransformID tf_id, MaterialID mat_id );
	
	CameraID AddCamera( const Camera::Data& data ) noexcept;
	const Camera* GetCamera( CameraID id ) const noexcept;
	Camera* ModifyCamera( CameraID id ) noexcept;

	LightID AddLight( const SceneLight::Data& data ) noexcept;
	const SceneLight* GetLight( LightID id ) const noexcept;
	SceneLight* ModifyLight( LightID id ) noexcept;

	StaticMeshInstance* ModifyInstance( MeshInstanceID id ) noexcept;
	ObjectTransform* ModifyTransform( TransformID id ) noexcept;

private:
	Scene* m_scene;
	StaticMeshManager* m_static_mesh_manager;
	TextureStreamer* m_tex_streamer;
	DynamicSceneBuffers* m_dynamic_buffers;
	MaterialTableBaker* m_material_table_baker;
};


// Manages scene lifetime, binds the scene to a render pipeline
class SceneManager
{
public:
	SceneManager( Microsoft::WRL::ComPtr<ID3D12Device> device, StagingDescriptorHeap* dsv_heap, size_t nframes_to_buffer, GPUTaskQueue* copy_queue );

	const SceneClientView& GetScene() const noexcept;
	SceneClientView& GetScene() noexcept;

	const DescriptorTableBakery& GetDescriptorTables() const noexcept;
	DescriptorTableBakery& GetDescriptorTables() noexcept;

	void UpdatePipelineBindings( CameraID main_camera_id );

	template<typename PipelineT>
	void BindToPipeline( PipelineT& pipeline );

	void FlushAllOperations();

private:

	void CleanModifiedItemsStatus();

	void ProcessSubmeshes();
	void CalcSubmeshBoundingBox( StaticSubmesh& submesh );

	Scene m_scene;
	DescriptorTableBakery m_gpu_descriptor_tables;
	StaticMeshManager m_static_mesh_mgr;
	TextureStreamer m_tex_streamer;
	SceneClientView m_scene_view;
	DynamicSceneBuffers m_dynamic_buffers;
	MaterialTableBaker m_material_table_baker;
	ShadowProvider m_shadow_provider;
	GPUTaskQueue* m_copy_queue;

	SceneCopyOp m_operation_counter = 0;
	GPUTaskQueue::Timestamp m_last_copy_timestamp = 0;

	const size_t m_nframes_to_buffer;

	// command objects
	std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> m_cmd_allocators;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_cmd_list;

	// temporary
	std::vector<RenderItem> m_lighting_items;
	CameraID m_main_camera_id = CameraID::nullid;
};

#include "SceneManager.hpp"