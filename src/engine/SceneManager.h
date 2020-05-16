#pragma once

#include "RenderData.h"
#include "Scene.h"

#include "StaticMeshManager.h"
#include "StaticTextureManager.h"
#include "TextureStreamer.h"
#include "CubemapManager.h"
#include "DynamicSceneBuffers.h"
#include "DescriptorTableBakery.h"
#include "MaterialTableBaker.h"
#include "ShadowProvider.h"
#include "UVScreenDensityCalculator.h"

#include <DirectXMath.h>
class SceneManager;
class GPUTaskQueue;

// update, remove and add methods for engine client
class SceneClientView
{
public:
    SceneClientView( Scene* scene,
                     StaticMeshManager* smm,
                     StaticTextureManager* stm,
                     TextureStreamer* tex_streamer,
                     CubemapManager* cubemap_manager,
                     DynamicSceneBuffers* dynamic_buffers,
                     MaterialTableBaker* material_table_baker )
        : m_scene( scene )
        , m_static_mesh_manager( smm )
        , m_static_texture_manager( stm )
        , m_cubemap_manager( cubemap_manager )
        , m_tex_streamer( tex_streamer )
        , m_dynamic_buffers( dynamic_buffers )
        , m_material_table_baker( material_table_baker ){}

    const Scene& GetROScene() const noexcept { return *m_scene; }

    World& GetWorld() noexcept { return m_scene->world; }
    const World& GetWorld() const noexcept { return m_scene->world; }

    StaticMeshID LoadStaticMesh( std::string name, std::vector<Vertex> vertices, std::vector<uint32_t> indices );
    TextureID LoadStreamedTexture( std::string path );
    TextureID LoadStaticTexture( std::string path );
    CubemapID LoadCubemap( std::string path );
    CubemapID AddCubemapFromTexture( TextureID tex_id );
    TransformID AddTransform( const DirectX::XMFLOAT4X4& obj2world = Identity4x4 );
    MaterialID AddMaterial( const MaterialPBR::TextureIds& textures, const DirectX::XMFLOAT3& diffuse_fresnel, const DirectX::XMFLOAT3& albedo_color, const DirectX::XMFLOAT4X4& uv_transform = Identity4x4 );
    StaticSubmeshID AddSubmesh( StaticMeshID mesh_id, const StaticSubmesh::Data& data );

    EnvMapID AddEnviromentMap( CubemapID cubemap_id, TransformID transform_id );
    EnvironmentMap* ModifyEnviromentMap( EnvMapID envmap_id ) noexcept;
    const EnvironmentMap* GetEnviromentMap( EnvMapID envmap_id ) const noexcept;
    
    CameraID AddCamera( const Camera::Data& data ) noexcept;
    const Camera* GetCamera( CameraID id ) const noexcept;
    Camera* ModifyCamera( CameraID id ) noexcept;

    LightID AddLight( const Light::Data& data ) noexcept;
    const Light* GetLight( LightID id ) const noexcept;
    Light* ModifyLight( LightID id ) noexcept;
    span<Light> GetAllLights() noexcept;

    ObjectTransform* ModifyTransform( TransformID id ) noexcept;

private:
    Scene* m_scene;
    StaticMeshManager* m_static_mesh_manager;
    StaticTextureManager* m_static_texture_manager;
    TextureStreamer* m_tex_streamer;
    DynamicSceneBuffers* m_dynamic_buffers;
    MaterialTableBaker* m_material_table_baker;
    CubemapManager* m_cubemap_manager;
};


// Manages scene lifetime, binds the scene to a render framegraph
class SceneManager
{
public:
    SceneManager( Microsoft::WRL::ComPtr<ID3D12Device> device, size_t nframes_to_buffer, GPUTaskQueue* copy_queue, GPUTaskQueue* graphics_queue );

    const SceneClientView& GetScene() const noexcept;
    SceneClientView& GetScene() noexcept;

    Scene& GetScene_Unsafe() noexcept { return m_scene; } // hack

    const DescriptorTableBakery& GetDescriptorTables() const noexcept;
    DescriptorTableBakery& GetDescriptorTables() noexcept;

    void UpdateFramegraphBindings( CameraID main_camera_id, const ParallelSplitShadowMapping& pssm, const D3D12_VIEWPORT& main_viewport );

    void FlushAllOperations();

    const TextureStreamer& GetTexStreamer() const noexcept { return m_tex_streamer; }

private:

    void CleanModifiedItemsStatus();

    void ProcessSubmeshes();
    void CalcSubmeshBoundingBox( StaticSubmesh& submesh );

    Scene m_scene;
    DescriptorTableBakery m_gpu_descriptor_tables;
    StaticMeshManager m_static_mesh_mgr;
    TextureStreamer m_tex_streamer;
    StaticTextureManager m_static_texture_mgr;
    CubemapManager m_cubemap_mgr;
    SceneClientView m_scene_view;
    DynamicSceneBuffers m_dynamic_buffers;
    MaterialTableBaker m_material_table_baker;
    UVScreenDensityCalculator m_uv_density_calculator;
    GPUTaskQueue* m_copy_queue;
    GPUTaskQueue* m_graphics_queue;

    SceneCopyOp m_operation_counter = 0;
    GPUTaskQueue::Timestamp m_last_copy_timestamp = 0;

    const size_t m_nframes_to_buffer;

    // command objects
    std::vector<std::pair<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, GPUTaskQueue::Timestamp>> m_copy_cmd_allocators;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_copy_cmd_list;

    std::vector<std::pair<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, GPUTaskQueue::Timestamp>> m_graphics_cmd_allocators;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_graphics_cmd_list;

    // temporary
    CameraID m_main_camera_id = CameraID::nullid;
};
