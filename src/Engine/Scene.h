#pragma once

#include "StdAfx.h"

#include "Assets.h"
#include "RHIUtils.h"
#include "Render/DebugDrawing.h"

SE_LOG_CATEGORY( Renderer );

class BlitTextureProgram;
class Rendergraph;
class RGTexture;
class DisplayMapping;

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

    std::span<const SceneMeshInstance> GetAllMeshInstances() const { return std::span<const SceneMeshInstance>( m_mesh_instances.data(), m_mesh_instances.data() + m_mesh_instances.size() ); }

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

    glm::vec3 m_eye_pos = ZeroVec3();
    glm::quat m_eye_orientation = IdentityQuat();
    glm::vec3 m_up = glm::vec3( 0, 1, 0 );

    float m_fovXRadians = glm::radians( 60.0f );

    glm::uvec2 m_cursor_position = glm::uvec2( -1, -1 );

    // for ping-pong. Not scalable, consider proper transient resources for rendergraph
    RHITexturePtr m_rt_frame[2] = { nullptr, nullptr };
    RHITextureRWViewPtr m_frame_rwview[2] = { nullptr, nullptr };
    RHITextureROViewPtr m_frame_roview[2] = { nullptr, nullptr };
    RHIRenderTargetViewPtr m_frame_rtview[2] = { nullptr, nullptr };

    DebugDrawingSceneViewData m_debug_draw_data;

public:
    SceneView( Scene* scene );

    void SetLookAt( const glm::vec3& eye, const glm::vec3& center );
    void SetExtents( const glm::uvec2& extents );

    glm::uvec2 GetExtent() const { return m_extents; }

    void SetFOV( float fovXRadians ) { m_fovXRadians = fovXRadians; }

    void SetCursorPosition( glm::uvec2 pos ) { m_cursor_position = pos; }
    glm::uvec2 GetCursorPosition() const { return m_cursor_position; }

    RHITexture* GetFrameColorTexture( uint32_t i ) const { return m_rt_frame[i].get(); }
    RHITextureROView* GetFrameColorTextureROView( uint32_t i ) const { return m_frame_roview[i].get(); }
    RHITextureRWView* GetFrameColorTextureRWView( uint32_t i ) const { return m_frame_rwview[i].get(); }
    RHIRenderTargetView* GetFrameColorTextureRTView( uint32_t i ) const { return m_frame_rtview[i].get(); }

    const DebugDrawingSceneViewData& GetDebugDrawData() const { return m_debug_draw_data; }

    glm::mat4x4 CalcViewMatrix() const;
    glm::mat4x4 CalcProjectionMatrix() const;

    Scene& GetScene() const { return *m_scene; }
};

// Data associated with a scene view managed by renderer for a single frame
struct SceneViewFrameData
{
    const SceneView* view = nullptr;
    RHIDescriptorSet* view_desc_set = nullptr;
    const RGTexture* scene_output[2] = {};
    int scene_output_idx = 0;
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

class GlobalDescriptors
{
private:
    RHIDescriptorSetLayoutPtr m_global_dsl;
    RHIDescriptorSetPtr m_global_descset;
    std::vector<uint32_t> m_free_geom_slots;
public:
    GlobalDescriptors();

    uint32_t AddGeometry( const RHIBufferViewInfo& vertices, const RHIBufferViewInfo& indices );

    RHIDescriptorSet& GetDescSet() const { return *m_global_descset; }
    RHIDescriptorSetLayout* GetLayout() const { return m_global_dsl.get(); }
};

class Renderer
{
private:

    std::unique_ptr<BlitTextureProgram> m_blit_texture_prog = nullptr;

    RHISamplerPtr m_point_sampler = nullptr;

    RHIDescriptorSetLayoutPtr m_rt_dsl = nullptr;
    RHIShaderBindingLayoutPtr m_rt_layout = nullptr;
    RHIRaytracingPipelinePtr m_rt_pipeline = nullptr;

    RHIDescriptorSetLayoutPtr m_view_dsl = nullptr;

    std::unique_ptr<DisplayMapping> m_display_mapping = nullptr;
    std::unique_ptr<DebugDrawing> m_debug_drawing = nullptr;

    std::unique_ptr<GlobalDescriptors> m_global_descriptors = nullptr;

    mutable std::atomic<uint32_t> m_random_seed = 0;

public:

    Renderer();
    ~Renderer();

    // ui_cmd_list is optional
    bool RenderScene( const RenderSceneParams& parms );

    void DebugUI();

    BlitTextureProgram* GetBlitTextureProgram() const;

    RHISampler* GetPointSampler() const;

    GlobalDescriptors& GetGlobalDescriptors() const { return *m_global_descriptors; }

    const DebugDrawing& GetDebugDrawing() const { return *m_debug_drawing; }

private:

    void CreatePrograms();

    void CreateSamplers();

    void CreateDescriptorSetLayout();

    void CreateRTPipeline();

    void UpdateSceneViewParams( const SceneViewFrameData& view_data );

    RHICommandList* CreateInitializedCommandList( RHI::QueueType queue_type ) const;

    void SetPSO( RHICommandList& cmd_list, const RHIRaytracingPipeline& rt_pso, const SceneViewFrameData& view_data ) const;
};