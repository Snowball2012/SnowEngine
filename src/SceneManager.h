#pragma once

#include "RenderData.h"
#include "Scene.h"

using SceneCopyOp = uint64_t;

#include "StaticMeshManager.h"
#include "TextureStreamer.h"

class SceneManager;
class StaticMeshManager;
class GPUTaskQueue;

// update, remove and add methods for scene
class SceneClientView
{
public:
	SceneClientView( Scene* scene, StaticMeshManager* smm, TextureStreamer* tex_streamer )
		: m_scene( scene )
		, m_static_mesh_manager( smm )
		, m_tex_streamer( tex_streamer ) {}

	const Scene& GetROScene() const noexcept { return *m_scene; }

	StaticMeshID LoadStaticMesh( std::string name, const span<const Vertex>& vertices, const span<const uint32_t>& indices );
	TextureID LoadStreamedTexture( std::string path );

private:
	Scene* m_scene;
	StaticMeshManager* m_static_mesh_manager;
	TextureStreamer* m_tex_streamer;
};


// Manages scene lifetime, binds the scene to a render pipeline
class SceneManager
{
public:
	SceneManager( Microsoft::WRL::ComPtr<ID3D12Device> device, size_t nframes_to_buffer, GPUTaskQueue* copy_queue );

	const SceneClientView& GetScene() const noexcept;
	SceneClientView& GetScene() noexcept;

	void UpdatePipelineBindings();

	template<typename PipelineT>
	void BindToPipeline( PipelineT* pipeline );

private:

	Scene m_scene;
	StaticMeshManager m_static_mesh_mgr;
	TextureStreamer m_tex_streamer;
	SceneClientView m_scene_view;
	GPUTaskQueue* m_copy_queue;

	SceneCopyOp m_operation_counter = 0;

	const size_t m_nframes_to_buffer;

	// command objects
	std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> m_cmd_allocators;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_cmd_list;
};

#include "SceneManager.hpp"