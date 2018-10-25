#pragma once

#include "RenderData.h"
#include "Scene.h"

using SceneCopyOp = uint64_t;

#include "StaticMeshManager.h"
#include "TextureStreamer.h"
#include "DynamicSceneBuffers.h"
#include "DescriptorTableBakery.h"
#include "MaterialTableBaker.h"

class SceneManager;
class GPUTaskQueue;

// update, remove and add methods for scene
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

	StaticMeshID LoadStaticMesh( std::string name, const span<const Vertex>& vertices, const span<const uint32_t>& indices );
	TextureID LoadStreamedTexture( std::string path );
	TransformID AddTransform( DirectX::XMFLOAT4X4 obj2world = Identity4x4 );
	MaterialID AddMaterial( const MaterialPBR::TextureIds& textures, DirectX::XMFLOAT3 diffuse_fresnel, DirectX::XMFLOAT4X4 uv_transform = Identity4x4 );

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
	SceneManager( Microsoft::WRL::ComPtr<ID3D12Device> device, size_t nframes_to_buffer, GPUTaskQueue* copy_queue );

	const SceneClientView& GetScene() const noexcept;
	SceneClientView& GetScene() noexcept;

	const DescriptorTableBakery& GetDescriptorTables() const noexcept;
	DescriptorTableBakery& GetDescriptorTables() noexcept;

	void UpdatePipelineBindings();

	template<typename PipelineT>
	void BindToPipeline( PipelineT* pipeline );

	void FlushAllOperations();

private:

	void CleanModifiedItemsStatus();

	Scene m_scene;
	DescriptorTableBakery m_gpu_descriptor_tables;
	StaticMeshManager m_static_mesh_mgr;
	TextureStreamer m_tex_streamer;
	SceneClientView m_scene_view;
	DynamicSceneBuffers m_dynamic_buffers;
	MaterialTableBaker m_material_table_baker;
	GPUTaskQueue* m_copy_queue;

	SceneCopyOp m_operation_counter = 0;
	GPUTaskQueue::Timestamp m_last_copy_timestamp = 0;

	const size_t m_nframes_to_buffer;

	// command objects
	std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> m_cmd_allocators;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_cmd_list;
};

#include "SceneManager.hpp"