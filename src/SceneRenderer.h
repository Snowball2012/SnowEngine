#pragma once

#include "GPUTaskQueue.h"
#include "StagingDescriptorHeap.h"
#include "CommandListPool.h"

#include "GPUResourceHolder.h"

#include "DepthPrepassNode.h"
#include "ShadowPassNode.h"
#include "PSSMNode.h"
#include "ForwardPassNode.h"
#include "SkyboxNode.h"
#include "BlurSSAONode.h"
#include "ToneMapNode.h"
#include "HBAONode.h"

#include "Scene.h"

#include "ForwardCBProvider.h"
#include "ShadowProvider.h"

#include "ParallelSplitShadowMapping.h"


class SceneRenderer
{
public:

    enum class RenderMode
    {
        FullTonemapped
    };

    struct HBAOSettings
    {
        float max_r = 0.20f;
        float angle_bias = DirectX::XM_PIDIV2 / 10.0f;
        int nsamples_per_direction = 4;
    };

    struct TonemapSettings
    {
        float max_luminance = 9000;
        float min_luminance = 1.e-2f;
    };

    struct DeviceContext
    {
        ID3D12Device* device = nullptr;
        DescriptorTableBakery* srv_cbv_uav_tables = nullptr;
    };

    struct Target
    {
        ID3D12Resource* resource;
        D3D12_CPU_DESCRIPTOR_HANDLE rtv;
        D3D12_VIEWPORT viewport;
        D3D12_RECT scissor_rect;
    };

    struct FrameContext
    {
        Target render_target;
        CommandListPool* cmd_list_pool;
    };

    struct SceneContext
    {
        const Camera::Data* main_camera;
        SkyboxData skybox;

        span<RenderItem_New> shadow_list;
        span<RenderItem_New> opaque_list;
        span<Light> light_list;

        DescriptorTableID ibl_table;
    };

    // factory
    static SceneRenderer Create( const DeviceContext& ctx, uint32_t width, uint32_t height, uint32_t n_frames_in_flight );
    
    ~SceneRenderer();

    SceneRenderer( const SceneRenderer& ) = delete;
    SceneRenderer& operator=( const SceneRenderer& ) = delete;
    SceneRenderer( SceneRenderer&& ) = default;
    SceneRenderer& operator=( SceneRenderer&& ) = default;

    // Target must be in D3D12_RESOURCE_STATE_RENDER_TARGET on graphics queue
    // Returned lists must be submitted to graphics queue, returned allocators
    // can be freed after the rendering is over on device queues
    void Draw( const SceneContext& scene_ctx, const FrameContext& frame_ctx, RenderMode mode,
               std::vector<CommandList>& graphics_cmd_lists,
               GPUResourceHolder& frame_resources );

    void SetTonemapSettings( const TonemapSettings& settings ) noexcept { m_tonemap_settings = settings; }
    TonemapSettings GetTonemapSettings() const noexcept { return m_tonemap_settings; }

    void SetHBAOSettings( const HBAOSettings& settings ) noexcept { m_hbao_settings = settings; }
    HBAOSettings GetHBAOSettings() const noexcept { return m_hbao_settings; }

    void SetSkybox( bool enable );

    ParallelSplitShadowMapping& GetPSSM() noexcept { return m_pssm; }

    // All queues used in Draw must be flushed before calling this method
    void SetInternalResolution( uint32_t width, uint32_t height );
    std::pair<uint32_t, uint32_t> GetInternalResolution() const noexcept { return std::make_pair( m_resolution_width, m_resolution_height ); }

    DXGI_FORMAT GetTargetFormat( RenderMode mode ) const noexcept;
    // May involve PSO recompilation
    void SetTargetFormat( RenderMode mode, DXGI_FORMAT format );

    static void MakeObjectCB( const DirectX::XMMATRIX& obj2world, GPUObjectConstants& object_cb );

private:
    // settings

    // internal resolution
    uint32_t m_resolution_width = std::numeric_limits<uint32_t>::max();
    uint32_t m_resolution_height = std::numeric_limits<uint32_t>::max();

    static constexpr DXGI_FORMAT DefaultBackbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT m_back_buffer_format = DefaultBackbufferFormat;

    HBAOSettings m_hbao_settings;
    TonemapSettings m_tonemap_settings;
    ParallelSplitShadowMapping m_pssm;

    // framegraph
    using FramegraphInstance =
        Framegraph
        <
            DepthPrepassNode,
            ShadowPassNode,
            PSSMNode,
            ForwardPassNode,
            SkyboxNode,
            HBAONode,
            BlurSSAONode,
            ToneMapNode
        >;
    FramegraphInstance m_framegraph;
    ShadowProvider m_shadow_provider;

    // transient resources
    DXGI_FORMAT m_depth_stencil_format_resource = DXGI_FORMAT_R32_TYPELESS;
    DXGI_FORMAT m_depth_stencil_format_dsv = DXGI_FORMAT_D32_FLOAT;
    DXGI_FORMAT m_depth_stencil_format_srv = DXGI_FORMAT_R32_FLOAT;
    DXGI_FORMAT m_hdr_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    DXGI_FORMAT m_normals_format = DXGI_FORMAT_R16G16_FLOAT;
    DXGI_FORMAT m_ssao_format = DXGI_FORMAT_R16_FLOAT;
    int32_t m_shadow_bias = 5000;

    std::unique_ptr<DynamicTexture> m_depth_stencil_buffer = nullptr;
    std::unique_ptr<DynamicTexture> m_hdr_backbuffer = nullptr;
    std::unique_ptr<DynamicTexture> m_hdr_ambient = nullptr;
    std::unique_ptr<DynamicTexture> m_normals = nullptr;
    std::unique_ptr<DynamicTexture> m_ssao = nullptr;
    std::unique_ptr<DynamicTexture> m_ssao_blurred = nullptr;
    std::unique_ptr<DynamicTexture> m_ssao_blurred_transposed = nullptr;

    StagingDescriptorHeap m_dsv_heap;
    StagingDescriptorHeap m_rtv_heap;

    // permanent context
    ID3D12Device* m_device = nullptr;
    DescriptorTableBakery* m_descriptor_tables = nullptr;

    SceneRenderer( const DeviceContext& ctx,
                   uint32_t width, uint32_t height,
                   StagingDescriptorHeap&& dsv_heap,
                   StagingDescriptorHeap&& rtv_heap,
                   ShadowProvider&& shadow_provider ) noexcept;


    void InitFramegraph();
    void CreateTransientResources();
    void ResizeTransientResources();

    D3D12_HEAP_DESC GetUploadHeapDescription() const;

    struct RenderBatchList
    {
        std::vector<RenderBatch> batches;
        ComPtr<ID3D12Resource> per_obj_cb;
    };
    RenderBatchList CreateRenderitems( const span<const RenderItem_New>& render_list, GPULinearAllocator& frame_allocator ) const;

    std::pair<Skybox, ComPtr<ID3D12Resource>> CreateSkybox( const SkyboxData& skybox, DescriptorTableID ibl_table, GPULinearAllocator& upload_cb_allocator ) const;

    ComPtr<ID3D12Resource> CreateSkyboxCB( const DirectX::XMFLOAT4X4& obj2world, GPULinearAllocator& upload_cb_allocator ) const;

    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle( DescriptorTableID id ) const { return m_descriptor_tables->GetTable( id )->gpu_handle; }
};