#pragma once

#include "StdAfx.h"

#include "Assets.h"
#include "RHIUtils.h"

SE_LOG_CATEGORY( Renderer );

class Rendergraph;
class RGTexture;
class DisplayMapping;
class DescriptorSetPool;

class SceneMeshInstance
{
public:
    // @todo - incapsulate as soon as it becomes more than simple getter/setter pair
    MeshAssetPtr m_asset = nullptr;

    Transform m_tf = {};

    TLAS::InstanceID m_tlas_instance = TLAS::InstanceID::nullid;
};
using SceneMeshInstanceList = packed_freelist<SceneMeshInstance>;
using SceneMeshInstanceID = SceneMeshInstanceList::id;

class Scene
{
private:
    std::unique_ptr<TLAS> m_tlas;
    SceneMeshInstanceList m_mesh_instances;

public:
    Scene();

    SceneMeshInstanceID AddMeshInstanceFromAsset( MeshAssetPtr base_asset );
    void RemoveMeshInstance( SceneMeshInstanceID id );
    SceneMeshInstance* GetMeshInstance( SceneMeshInstanceID id ) { return m_mesh_instances.try_get( id ); };
    const SceneMeshInstance* GetMeshInstance( SceneMeshInstanceID id ) const { return m_mesh_instances.try_get( id ); }

    TLAS& GetTLAS() { return *m_tlas; }

    // Call before any render operation on the scene. This makes changes made to scene objects (mesh instances, etc.) visible to the renderer
    void Synchronize();

private:
    void UpdateTLASTransforms();
};

class SceneView
{
private:
    Scene* m_scene = nullptr;

    glm::uvec2 m_extents = glm::uvec2( 0, 0 );
    RHITexturePtr m_frame_output = nullptr;

    glm::vec3 m_eye_pos = ZeroVec3();
    glm::quat m_eye_orientation = IdentityQuat();
    glm::vec3 m_up = glm::vec3( 0, 1, 0 );

    float m_fovXRadians = glm::radians( 60.0f );

    RHITexturePtr m_rt_frame = nullptr;
    RHITextureRWViewPtr m_frame_rwview = nullptr;
    RHITextureROViewPtr m_frame_roview = nullptr;

public:
    SceneView( Scene* scene );

    void SetLookAt( const glm::vec3& eye, const glm::vec3& center );
    void SetExtents( const glm::uvec2& extents );

    glm::uvec2 GetExtent() const { return m_extents; }

    void SetFOV( float fovXRadians ) { m_fovXRadians = fovXRadians; }

    RHITexture* GetFrameColorTexture() const { return m_rt_frame.get(); }
    RHITextureROView* GetFrameColorTextureROView() const { return m_frame_roview.get(); }
    RHITextureRWView* GetFrameColorTextureRWView() const { return m_frame_rwview.get(); }

    glm::mat4x4 CalcViewMatrix() const;
    glm::mat4x4 CalcProjectionMatrix() const;

    Scene& GetScene() const { return *m_scene; }
};

// Data associated with a scene view managed by renderer for a single frame
struct SceneViewFrameData
{
    const SceneView* view = nullptr;
    RHIDescriptorSet* view_desc_set = nullptr;
    const RGTexture* scene_output = nullptr;
    Rendergraph* rg = nullptr;
};

class ISceneRenderExtension
{
public:
    virtual void PostSetupRendergraph( const SceneViewFrameData& data ) = 0;
    virtual void PostCompileRendergraph( const SceneViewFrameData& data ) = 0;
};

struct RenderSceneParams
{
    SceneView* view = nullptr;
    Rendergraph* rg = nullptr;
    ISceneRenderExtension* extension = nullptr; // optional, allows to hook into scene rendering process (add ui passes / blit to swapchain, for example)
};

class Renderer
{
private:

    static constexpr uint32_t NumBufferizedFrames = 3;

    RHIDescriptorSetLayoutPtr m_rt_dsl = nullptr;
    RHIShaderBindingLayoutPtr m_rt_layout = nullptr;
    RHIRaytracingPipelinePtr m_rt_pipeline = nullptr;

    RHIDescriptorSetLayoutPtr m_view_dsl = nullptr;

    std::vector<RHIUploadBufferPtr> m_view_buffers;
    std::vector<RHIUniformBufferViewPtr> m_view_buffer_views;

    std::vector<DescriptorSetPool> m_frame_descriptors;

    std::unique_ptr<DisplayMapping> m_display_mapping = nullptr;

    uint64_t m_frame_idx = 0;

public:

    Renderer();
    ~Renderer();

    void NextFrame();

    // ui_cmd_list is optional
    bool RenderScene( const RenderSceneParams& parms );

    // returned descriptor may only be used until NextFrame() is called
    RHIDescriptorSet* AllocateFrameDescSet( RHIDescriptorSetLayout& layout );

    void DebugUI();

private:

    void CreateDescriptorSetLayout();
    void CreateSceneViewParamsBuffers();
    void CreateDescriptorSets();

    void CreateRTPipeline();

    void UpdateSceneViewParams( const SceneViewFrameData& view_data );

    RHICommandList* CreateInitializedCommandList( RHI::QueueType queue_type ) const;

    void SetPSO( RHICommandList& cmd_list, const RHIRaytracingPipeline& rt_pso, const SceneViewFrameData& view_data ) const;

    uint32_t GetCurrFrameBufIndex() const;
};