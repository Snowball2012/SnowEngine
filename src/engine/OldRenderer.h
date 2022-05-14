#pragma once

#include "RenderData.h"
#include "SceneImporter.h"

#include "StagingDescriptorHeap.h"

#include "ParallelSplitShadowMapping.h"

#include "GPUTaskQueue.h"
#include "GPUResourceHolder.h"

#include "SceneManager.h"

#include "Renderer.h"
#include "resources/GPUDevice.h"

class Renderer;

// throws SnowEngineExceptions and DxExceptions for non-recoverable faults
class OldRenderer
{
public:
    
    OldRenderer( HWND main_hwnd, uint32_t screen_width, uint32_t screen_height );
    ~OldRenderer();

    void Init( );
    
    struct Context
    {
        bool wireframe_mode;
        bool taa_enabled;
    };
    void Draw(float time);
    void NewGUIFrame();
    void Resize( uint32_t new_width, uint32_t new_height );

    // getters/setters
    D3D12_VIEWPORT& ScreenViewport() { return m_screen_viewport; }
    const D3D12_VIEWPORT& ScreenViewport() const { return m_screen_viewport; }
    D3D12_RECT& ScissorRect() { return m_scissor_rect; }
    const D3D12_RECT& ScissorRect() const { return m_scissor_rect; }

    Renderer::TonemapSettings m_tonemap_settings;

    HBAOPass::Settings m_hbao_settings;
    ParallelSplitShadowMapping& PSSM() noexcept;

    SceneClientView& GetScene() { return m_scene_manager->GetScene(); }

    // returns false if camera doesn't exist
    bool SetMainCamera( World::Entity camera );
    bool SetFrustumCullCamera( World::Entity camera ); // Renderer will use main camera if this one is not specified, mainly for debug purposes

    bool SetIrradianceMap( CubemapID id );
    bool SetReflectionProbe( CubemapID id );

    void SetSkybox( bool enable );

    struct PerformanceStats
    {
        TextureStreamer::Stats tex_streamer;
        uint64_t num_renderitems_total;
        uint64_t num_renderitems_to_draw;
    };
    
    PerformanceStats GetPerformanceStats() const noexcept;

private:
    using DescriptorTableID = DescriptorTableBakery::TableID;

    // windows stuff
    HWND m_main_hwnd = nullptr;

    // d3d stuff
    ComPtr<IDXGIFactory4> m_dxgi_factory = nullptr;
    std::unique_ptr<GPUDevice> m_gpu_device = nullptr;

    uint32_t m_client_width;
    uint32_t m_client_height;

    ComPtr<IDXGISwapChain> m_swap_chain = nullptr;
    static constexpr int SwapChainBufferCount = 2;
    int m_curr_back_buff = 0;
    ComPtr<ID3D12Resource> m_swap_chain_buffer[SwapChainBufferCount] = { nullptr };
    std::optional<Descriptor> m_back_buffer_rtv[SwapChainBufferCount] = { std::nullopt };

    
    DXGI_FORMAT m_back_buffer_format = DXGI_FORMAT_R8G8B8A8_UNORM;
    D3D12_VIEWPORT m_screen_viewport;
    D3D12_RECT m_scissor_rect;

    StaticMeshID m_geom_id = StaticMeshID::nullid;
    MaterialID m_placeholder_material = MaterialID::nullid;
    World::Entity m_main_camera = World::Entity::nullid;
    World::Entity m_frustum_cull_camera = World::Entity::nullid;

    CubemapID m_irradiance_map = CubemapID::nullid;
    CubemapID m_reflection_probe = CubemapID::nullid;
    TextureID m_brdf_lut = TextureID::nullid;
    DescriptorTableID m_ibl_table = DescriptorTableID::nullid;

    std::unique_ptr<SceneManager> m_scene_manager;

    ComPtr<ID3D12Resource> m_tlas;

    // descriptor heaps
    std::unique_ptr<DescriptorHeap> m_srv_ui_heap = nullptr;
    std::unique_ptr<StagingDescriptorHeap> m_rtv_heap = nullptr;

    // ui font descriptor
    std::unique_ptr<Descriptor> m_ui_font_desc = nullptr;

    // resources
    static constexpr int FrameResourceCount = 3;
    std::vector<std::pair<GPUTaskQueue::Timestamp, GPUResourceHolder>> m_frame_resources;
    std::pair<GPUTaskQueue::Timestamp, GPUResourceHolder>* m_cur_frame_resource = nullptr;
    int m_cur_fr_idx = 0;

    std::unique_ptr<Renderer> m_renderer = nullptr;

    // stats
    uint64_t m_num_renderitems_total = 0; // last frame
    uint64_t m_num_renderitems_to_draw = 0; // last frame

    // methods
    void CreateDevice();
    void CreateSwapChain();
    void RecreateSwapChain( uint32_t new_width, uint32_t new_height );

    void BuildRtvDescriptorHeaps();
    void BuildUIDescriptorHeap( );
    void InitImgui();

    void BuildFrameResources();

    void EndFrame(); // call at the end of the frame to wait for next available frame resource

	using RenderItemStorage = std::vector<std::vector<RenderItem>>;
    RenderItemStorage CreateRenderItems( const RenderTask& task, const DirectX::XMVECTOR& camera_pos ); // returns a storage for renderitems, keep it until the data is submitted to GPU

    void RebuildTLAS();
    
    ID3D12Resource* CurrentBackBuffer();
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
    const DescriptorTableBakery& DescriptorTables() const { return m_scene_manager->GetDescriptorTables(); }
    DescriptorTableBakery& DescriptorTables() { return m_scene_manager->GetDescriptorTables(); }
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle( DescriptorTableID id ) const { return DescriptorTables().GetTable( id )->gpu_handle; }
};
