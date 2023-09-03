#pragma once

#include <Core/Core.h>

class RHISwapChain;
class RHISemaphore;
class RHICommandList;
struct RHISwapChainCreateInfo;
class RHIShader;
class RHIBuffer;
class RHITexture;
class RHIUploadBuffer;
class RHIASInstanceBuffer;
class RHIAccelerationStructure;
class RHIDescriptorSetLayout;
class RHIDescriptorSet;
class RHIShaderBindingLayout;
class RHIGraphicsPipeline;
class RHIRaytracingPipeline;
class RHIUniformBufferView;
class RHITextureROView;
class RHITextureRWView;
class RHIRenderTargetView;
class RHISampler;
struct RHIASGeometryInfo;
struct RHIASInstanceData;
struct RHIGraphicsPipelineInfo;
struct RHIRaytracingPipelineInfo;

struct RHIFence
{
    uint64_t _1;
    uint64_t _2;
    uint64_t _3;
};

enum class RHIBufferUsageFlags : uint32_t
{
    None = 0,
    TransferSrc = 0x1,
    TransferDst = 0x2,
    VertexBuffer = 0x4,
    IndexBuffer = 0x8,
    UniformBuffer = 0x10,
    AccelerationStructure = 0x20,
    AccelerationStructureInput = 0x40,
    AccelerationStructureScratch = 0x80,
    ShaderBindingTable = 0x100,

    NumFlags = 9
};
IMPLEMENT_SCOPED_ENUM_FLAGS( RHIBufferUsageFlags )

enum class RHIPipelineStageFlags : uint32_t
{
    ColorAttachmentOutput = 0x00000001,
    VertexShader = 0x00000002,
    PixelShader = 0x00000004,

    AllBits = 0x00000007,

    NumFlags = 3,
};
IMPLEMENT_SCOPED_ENUM_FLAGS( RHIPipelineStageFlags )

enum class RHIShaderStageFlags : uint32_t
{
    VertexShader = 0x00000001,
    PixelShader = 0x00000002,
    RaygenShader = 0x00000004,
    MissShader = 0x00000008,

    AllBits = 0x0000000f,

    NumFlags = 4,
};
IMPLEMENT_SCOPED_ENUM_FLAGS( RHIShaderStageFlags )

enum class RHITextureDimensions : uint8_t
{
    T1D,
    T2D,
    T3D
};

enum class RHITextureUsageFlags : uint32_t
{
    None = 0,

    TransferSrc = 0x1,
    TransferDst = 0x2,
    TextureROView = 0x4,
    RenderTargetView = 0x8,
    TextureRWView = 0x10,

    NumFlags = 5
};
IMPLEMENT_SCOPED_ENUM_FLAGS( RHITextureUsageFlags )

enum class RHIFormat : uint8_t
{
    Undefined = 0,
    R32G32_SFLOAT,
    R32G32B32_SFLOAT,
    B8G8R8A8_SRGB,
    R8G8B8A8_SRGB,
    R8G8B8A8_UNORM,
    RGB9E5,

    // Beware that some implementation don't support this format. @todo - query format support form rhi if really needed
    RGB8_SRGB
};

enum class RHIShaderBindingType : uint8_t
{
    UniformBuffer = 0,
    TextureRO,
    AccelerationStructure,
    TextureRW,
    Sampler
};

enum class RHITextureLayout : uint8_t
{
    Undefined = 0,
    ShaderReadOnly,
    ShaderReadWrite,
    RenderTarget,
    TransferSrc,
    TransferDst,
    Present
};

enum class RHICullModeFlags
{
    None = 0,

    Front = 0x1,
    Back = 0x2,

    All = 0x3,

    NumFlags = 2
};
IMPLEMENT_SCOPED_ENUM_FLAGS( RHICullModeFlags )

enum class RHIAccelerationStructureType : uint8_t
{
    BLAS = 0,
    TLAS
};

struct RHIASBuildSizes
{
    size_t as_size = 0;
    size_t scratch_size = 0;
};

struct RHIUniformBufferViewInfo
{
    static constexpr uint64_t WHOLE_SIZE = -1;

    RHIBuffer* buffer = nullptr;
    uint64_t offset = 0;
    uint64_t range = WHOLE_SIZE;
};

class RHI
{
public:
    virtual ~RHI() {};

    virtual bool SupportsRaytracing() const { return false; }

    // window_handle must outlive the swap chain
    virtual RHISwapChain* CreateSwapChain( const RHISwapChainCreateInfo& create_info ) { NOTIMPL; return nullptr; }
    virtual RHISwapChain* GetMainSwapChain() { NOTIMPL; return nullptr; }

    virtual RHISemaphore* CreateGPUSemaphore() { NOTIMPL; return nullptr; }

    struct PresentInfo
    {
        class RHISemaphore** wait_semaphores = nullptr;
        size_t semaphore_count = 0;
    };
    virtual void Present( RHISwapChain& swap_chain, const PresentInfo& info ) { NOTIMPL; }

    virtual void WaitIdle() { NOTIMPL; };

    enum class QueueType : uint8_t
    {
        Graphics = 0,
        Count
    };

    virtual RHICommandList* GetCommandList( QueueType type ) { NOTIMPL; return nullptr; }

    // command lists here must belong to the same QueueType
    struct SubmitInfo
    {
        size_t cmd_list_count = 0;
        class RHICommandList* const* cmd_lists = nullptr;
        size_t wait_semaphore_count = 0;
        class RHISemaphore* const* semaphores_to_wait = nullptr;
        const RHIPipelineStageFlags* stages_to_wait = nullptr;
        class RHISemaphore* semaphore_to_signal = nullptr; // optional
    };
    virtual RHIFence SubmitCommandLists( const SubmitInfo& info ) { NOTIMPL; }

    virtual void WaitForFenceCompletion( const RHIFence& fence ) { NOTIMPL; }

    enum class ShaderFrequency : uint8_t
    {
        Vertex = 0,
        Pixel,
        Compute,
        Raygen,
        Miss,
        ClosestHit,
        Count
    };

    struct ShaderDefine
    {
        const char* name = nullptr;
        const char* value = nullptr;
    };
    struct ShaderCreateInfo
    {
        const wchar_t* filename = nullptr;
        ShaderFrequency frequency = ShaderFrequency::Vertex;
        const char* entry_point = nullptr;
        const ShaderDefine* defines = nullptr;
        size_t define_count = 0;
    };
    virtual RHIShader* CreateShader( const ShaderCreateInfo& shader_info ) { NOTIMPL; return nullptr; }


    struct DescriptorViewRange
    {
        RHIShaderBindingType type = RHIShaderBindingType::UniformBuffer;
        int32_t count = 1; // negative means unbound bindless
        RHIShaderStageFlags stages = RHIShaderStageFlags::AllBits; // used only in CreateShaderBindingLayout
    };

    struct DescriptorSetLayoutInfo
    {
        const DescriptorViewRange* ranges = nullptr;
        size_t range_count = 0;
    };
    virtual RHIDescriptorSetLayout* CreateDescriptorSetLayout( const DescriptorSetLayoutInfo& info ) { NOTIMPL; return nullptr; }

    virtual RHIDescriptorSet* CreateDescriptorSet( RHIDescriptorSetLayout& layout ) { NOTIMPL; return nullptr; }

    struct ShaderBindingLayoutInfo
    {
        RHIDescriptorSetLayout* const* tables = nullptr;
        size_t table_count = 0;

        // Simplify push constants usage. We only have one contiguous push constant buffer internaly
        size_t push_constants_size = 0;
    };

    virtual RHIShaderBindingLayout* CreateShaderBindingLayout( const ShaderBindingLayoutInfo& layout_info ) { NOTIMPL; return nullptr; }

    virtual RHIGraphicsPipeline* CreatePSO( const RHIGraphicsPipelineInfo& pso_info ) { NOTIMPL; return nullptr; }

    virtual RHIRaytracingPipeline* CreatePSO( const RHIRaytracingPipelineInfo& pso_info ) { NOTIMPL; return nullptr; }

    struct BufferInfo
    {
        uint64_t size = 0;
        RHIBufferUsageFlags usage = RHIBufferUsageFlags::None;
    };
    virtual RHIUploadBuffer* CreateUploadBuffer( const BufferInfo& info ) { NOTIMPL; return nullptr; }
    virtual RHIBuffer* CreateDeviceBuffer( const BufferInfo& info ) { NOTIMPL; return nullptr; }
    
    struct ASInstanceBufferInfo
    {
        const RHIASInstanceData* data = nullptr;
        size_t num_instances = 0;
    };
    virtual RHIASInstanceBuffer* CreateASInstanceBuffer( const ASInstanceBufferInfo& info ) { NOTIMPL; return nullptr; }

    struct TextureInfo
    {
        RHITextureDimensions dimensions = RHITextureDimensions::T2D;
        RHIFormat format = RHIFormat::Undefined;
        bool allow_multiformat_views = false;

        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t depth = 0;

        uint32_t mips = 0;
        uint32_t array_layers = 0;

        RHITextureUsageFlags usage = RHITextureUsageFlags::None;

        RHITextureLayout initial_layout = RHITextureLayout::Undefined;

        RHI::QueueType initial_queue = RHI::QueueType::Graphics;
    };
    virtual RHITexture* CreateTexture( const TextureInfo& info ) { NOTIMPL; return nullptr; }

    virtual RHIUniformBufferView* CreateUniformBufferView( const RHIUniformBufferViewInfo& info ) { NOTIMPL; return nullptr; }

    struct TextureROViewInfo
    {
        RHITexture* texture = nullptr;
    };
    virtual RHITextureROView* CreateTextureROView( const TextureROViewInfo& info ) { NOTIMPL; return nullptr; }

    struct TextureRWViewInfo
    {
        RHITexture* texture = nullptr;
        RHIFormat format = RHIFormat::Undefined;
    };
    virtual RHITextureRWView* CreateTextureRWView( const TextureRWViewInfo& info ) { NOTIMPL; return nullptr; }

    struct RenderTargetViewInfo
    {
        RHITexture* texture = nullptr;
        RHIFormat format = RHIFormat::Undefined;
    };
    virtual RHIRenderTargetView* CreateRTV( const RenderTargetViewInfo& info ) { NOTIMPL; return nullptr; }

    struct SamplerInfo
    {
        float mip_bias = 0.0f;
        bool enable_anisotropy = false;
    };
    virtual RHISampler* CreateSampler( const SamplerInfo& info ) { NOTIMPL; return nullptr; }

    struct ASInfo
    {
        RHIAccelerationStructureType type = RHIAccelerationStructureType::BLAS;
        size_t size = 0;
    };
    virtual RHIAccelerationStructure* CreateAS( const ASInfo& info ) { NOTIMPL; return nullptr; }

    virtual bool GetASBuildSize( RHIAccelerationStructureType type, const RHIASGeometryInfo* geom_infos, size_t num_geoms, RHIASBuildSizes& out_sizes ) { NOTIMPL; return false; }

    // Causes a full pipeline flush
    virtual bool ReloadAllShaders() { NOTIMPL; return false; }
};

class RHIObject
{
public:
    virtual void AddRef() = 0;
    virtual void Release() = 0;

    virtual ~RHIObject() {}
};

// helpers for reference counting
template<typename T>
using RHIObjectPtr = boost::intrusive_ptr<T>;

inline void intrusive_ptr_add_ref( RHIObject* p )
{
    if ( p )
        p->AddRef();
}
inline void intrusive_ptr_release( RHIObject* p )
{
    if ( p )
        p->Release();
}

struct RHISwapChainCreateInfo
{
    void* window_handle = nullptr;
    uint32_t surface_num = 0;
};

class RHISwapChain : public RHIObject
{
public:
    virtual ~RHISwapChain() { }

    virtual glm::uvec2 GetExtent() const { NOTIMPL; }

    virtual void AcquireNextImage( RHISemaphore* semaphore_to_signal, bool& out_recreated ) { NOTIMPL; }

    virtual void Recreate() { NOTIMPL; }

    virtual RHIFormat GetFormat() const { NOTIMPL; return RHIFormat::Undefined; }

    virtual RHITexture* GetTexture() { NOTIMPL; return nullptr; }

    virtual RHIRenderTargetView* GetRTV() { NOTIMPL; return nullptr; }
};

class RHISemaphore : public RHIObject
{
public:
    virtual ~RHISemaphore() {}
};

enum class RHIIndexBufferType
{
    UInt16,
    UInt32
};

struct RHITextureSubresourceRange
{
    static constexpr uint32_t ALL_MIPS = -1;
    static constexpr uint32_t ALL_LAYERS = -1;

    uint32_t mip_base = 0;
    uint32_t mip_count = ALL_MIPS;
    uint32_t array_base = 0;
    uint32_t array_count = ALL_LAYERS;
};

struct RHITextureBarrier
{
    const RHITexture* texture = nullptr;
    RHITextureSubresourceRange subresources;
    RHITextureLayout layout_src = RHITextureLayout::Undefined;
    RHITextureLayout layout_dst = RHITextureLayout::Undefined;
};

struct RHIBufferTextureCopyRegion
{
    size_t buffer_offset = 0;
    size_t buffer_row_stride = 0; // 0 means tightly packed
    size_t buffer_texture_height = 0; // for 3d texture copies. 0 means tightly packed
    size_t texture_offset[3] = { 0,0,0 };
    size_t texture_extent[3] = { 0,0,0 };
    RHITextureSubresourceRange texture_subresource = {};
};

struct RHITextureTextureCopyRegion
{

};

struct RHIViewport
{
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float min_depth = 0.0f;
    float max_depth = 0.0f;
};

struct RHIRect2D
{
    glm::ivec2 offset = glm::ivec2( 0, 0 );
    glm::uvec2 extent = glm::uvec2( 0, 0 );
};

enum class RHILoadOp : uint8_t
{
    DontCare = 0,
    PreserveContents,
    Clear
};

enum class RHIStoreOp : uint8_t
{
    DontCare = 0,
    Store
};

union RHIClearColorValue
{
    float float32[4];
    int32_t int32[4];
    uint32_t uint32[4];
};

struct RHIPassRTVInfo
{
    RHIRenderTargetView* rtv = nullptr;
    RHILoadOp load_op = RHILoadOp::DontCare;
    RHIStoreOp store_op = RHIStoreOp::DontCare;
    RHIClearColorValue clear_value = { {0,0,0,0} };
};

struct RHIPassInfo
{
    const RHIPassRTVInfo* render_targets = nullptr;
    size_t render_targets_count = 0;

    RHIRect2D render_area = {};
};

enum class RHIASGeometryType : uint8_t
{
    Triangles,
    Instances
};

struct RHIASTrianglesInfo
{
    RHIFormat vtx_format = RHIFormat::Undefined;
    const RHIBuffer* vtx_buf = nullptr;
    size_t vtx_stride = 0;
    RHIIndexBufferType idx_type = RHIIndexBufferType::UInt16;
    const RHIBuffer* idx_buf = nullptr;
};

struct RHIASInstanceData
{
    const RHIAccelerationStructure* blas = nullptr;
    glm::mat3x4 transform = {};
};

struct RHIASInstancesInfo
{
    const RHIASInstanceBuffer* instance_buf = nullptr;
    size_t num_instances = 0;
};

struct RHIASGeometryInfo
{
    RHIASGeometryType type = RHIASGeometryType::Triangles;
    union
    {
        RHIASTrianglesInfo triangles;
        RHIASInstancesInfo instances;
    };
};

struct RHIASBuildInfo
{
    const RHIBuffer* scratch = nullptr;
    const RHIAccelerationStructure* dst = nullptr;
    const RHIASGeometryInfo** geoms = nullptr;
    size_t geoms_count = 0;
};

class RHICommandList
{
public:
    virtual ~RHICommandList() {}

    virtual RHI::QueueType GetType() const { NOTIMPL; }

    virtual void Begin() { NOTIMPL; }
    virtual void End() { NOTIMPL; }

    struct CopyRegion
    {
        size_t src_offset = 0;
        size_t dst_offset = 0;
        size_t size = 0;
    };
    virtual void CopyBuffer( RHIBuffer& src, RHIBuffer& dst, size_t region_count, CopyRegion* regions ) { NOTIMPL; }

    virtual void Draw(
        uint32_t vertex_count,
        uint32_t instance_count,
        uint32_t first_vertex,
        uint32_t first_instance )
    {
        NOTIMPL;
    }

    virtual void DrawIndexed(
        uint32_t index_count,
        uint32_t instance_count,
        uint32_t first_index,
        int32_t vertex_offset,
        uint32_t first_instance )
    {
        NOTIMPL;
    }

    virtual void TraceRays( glm::uvec3 threads_count ) { NOTIMPL; }

    virtual void SetPSO( const RHIGraphicsPipeline& pso ) { NOTIMPL; }
    virtual void SetPSO( const RHIRaytracingPipeline& pso ) { NOTIMPL; }

    virtual void BindDescriptorSet( size_t slot_idx, RHIDescriptorSet& set ) { NOTIMPL; }

    virtual void SetVertexBuffers( uint32_t first_binding, const RHIBuffer* buffers, size_t buffers_count, const size_t* opt_offsets ) { NOTIMPL; }
    virtual void SetIndexBuffer( const  RHIBuffer& index_buf, RHIIndexBufferType type, size_t offset ) { NOTIMPL; }

    virtual void TextureBarriers( const RHITextureBarrier* barriers, size_t barrier_count ) { NOTIMPL; }

    virtual void BeginPass( const RHIPassInfo& pass_info ) { NOTIMPL; }
    virtual void EndPass() { NOTIMPL; }

    virtual void CopyBufferToTexture(
        const RHIBuffer& buf, RHITexture& texture,
        const RHIBufferTextureCopyRegion* regions, size_t region_count ) {
        NOTIMPL;
    }

    virtual void CopyTextureToTexture(
        const RHITexture& src, const RHITexture& dst,
        const RHITextureTextureCopyRegion* regions, size_t region_count ) {
        NOTIMPL;
    }

    virtual void SetViewports( size_t first_viewport, const RHIViewport* viewports, size_t viewports_count ) { NOTIMPL; }
    virtual void SetScissors( size_t first_scissor, const RHIRect2D* scissors, size_t scissors_count ) { NOTIMPL; }

    virtual void PushConstants( size_t offset, const void* data, size_t size ) { NOTIMPL; }

    virtual void BuildAS( const RHIASBuildInfo& info ) { NOTIMPL; }
};

class RHIShader : public RHIObject
{
public:
    virtual ~RHIShader() {}
};
using RHIShaderPtr = RHIObjectPtr<RHIShader>;

class RHIGraphicsPipeline : public RHIObject
{
public:
    virtual ~RHIGraphicsPipeline() {}
};
using RHIGraphicsPipelinePtr = RHIObjectPtr<RHIGraphicsPipeline>;

class RHIRaytracingPipeline : public RHIObject
{
public:
    virtual ~RHIRaytracingPipeline() {}
};
using RHIRaytracingPipelinePtr = RHIObjectPtr<RHIRaytracingPipeline>;

class RHIShaderBindingLayout : public RHIObject
{
public:
    virtual ~RHIShaderBindingLayout() {}
};
using RHIShaderBindingLayoutPtr = RHIObjectPtr<RHIShaderBindingLayout>;

class RHIDescriptorSet : public RHIObject
{
public:
    virtual void BindUniformBufferView( size_t range_idx, size_t idx_in_range, RHIUniformBufferView& cbv ) { NOTIMPL; }
    virtual void BindUniformBufferView( size_t range_idx, size_t idx_in_range, const RHIUniformBufferViewInfo& cbv ) { NOTIMPL; }
    virtual void BindTextureROView( size_t range_idx, size_t idx_in_range, RHITextureROView& srv ) { NOTIMPL; }
    virtual void BindTextureRWView( size_t range_idx, size_t idx_in_range, RHITextureRWView& srv ) { NOTIMPL; }
    virtual void BindAccelerationStructure( size_t range_idx, size_t idx_in_range, RHIAccelerationStructure& as ) { NOTIMPL; }
    virtual void BindSampler( size_t range_idx, size_t idx_in_range, RHISampler& sampler ) { NOTIMPL; }

    virtual void FlushBinds() { NOTIMPL; }

    virtual ~RHIDescriptorSet() {}
};
using RHIDescriptorSetPtr = RHIObjectPtr<RHIDescriptorSet>;

class RHIDescriptorSetLayout : public RHIObject
{
public:
    virtual ~RHIDescriptorSetLayout() {}
};
using RHIDescriptorSetLayoutPtr = RHIObjectPtr<RHIDescriptorSetLayout>;

enum class RHIPrimitiveFrequency : uint8_t
{
    PerVertex = 0,
    PerInstance = 1
};

struct RHIPrimitiveAttributeInfo
{
    const char* semantic = nullptr;
    RHIFormat format = RHIFormat::Undefined;
    uint32_t offset = 0;
};

struct RHIPrimitiveBufferLayout
{
    const RHIPrimitiveAttributeInfo* attributes = nullptr;
    size_t attributes_count = 0;
    size_t stride = 0;
};

struct RHIInputAssemblerInfo
{
    const RHIPrimitiveBufferLayout** primitive_buffers = nullptr;
    const RHIPrimitiveFrequency* frequencies = nullptr;
    size_t buffers_count = 0;
};

struct RHIRasterizerInfo
{
    RHICullModeFlags cull_mode = RHICullModeFlags::None;
};

enum class RHIBlendFactor
{
    Zero = 0,
    One,
    SrcAlpha,
    OneMinusSrcAlpha
};

struct RHIPipelineRTInfo
{
    RHIFormat format = RHIFormat::Undefined;

    bool enable_blend = false;

    RHIBlendFactor src_color_blend = RHIBlendFactor::One;
    RHIBlendFactor dst_color_blend = RHIBlendFactor::OneMinusSrcAlpha;

    RHIBlendFactor src_alpha_blend = RHIBlendFactor::One;
    RHIBlendFactor dst_alpha_blend = RHIBlendFactor::OneMinusSrcAlpha;
};

struct RHIGraphicsPipelineInfo
{
    const RHIInputAssemblerInfo* input_assembler = nullptr;

    RHIRasterizerInfo rasterizer = {};

    RHIShader* vs = nullptr;
    RHIShader* ps = nullptr;

    RHIShaderBindingLayout* binding_layout = nullptr;

    const RHIPipelineRTInfo* rt_info = nullptr;
    size_t rts_count = 0;
};

struct RHIRaytracingPipelineInfo
{
    RHIShader* raygen_shader = nullptr;

    RHIShader* miss_shader = nullptr;

    RHIShader* closest_hit_shader = nullptr;

    RHIShaderBindingLayout* binding_layout = nullptr;
};

class RHIBuffer : public RHIObject
{
public:
    virtual ~RHIBuffer() override {}

    virtual size_t GetSize() const { return 0; }
};
using RHIBufferPtr = RHIObjectPtr<RHIBuffer>;

class RHIUploadBuffer : public RHIObject
{
public:
    virtual ~RHIUploadBuffer() override {}

    // For rhi needs
    virtual RHIBuffer* GetBuffer() { NOTIMPL; return nullptr; }
    virtual const RHIBuffer* GetBuffer() const { NOTIMPL; return nullptr; }

    virtual void WriteBytes( const void* src, size_t size, size_t offset = 0 ) { NOTIMPL; }
};
using RHIUploadBufferPtr = RHIObjectPtr<RHIUploadBuffer>;

class RHIASInstanceBuffer : public RHIObject
{
public:
    virtual ~RHIASInstanceBuffer() override {}

    virtual void UpdateBuffer( const RHIASInstanceData* data, size_t num_instances ) { NOTIMPL; }
};
using RHIASInstanceBufferPtr = RHIObjectPtr<RHIASInstanceBuffer>;

class RHITexture : public RHIObject
{
public:
    virtual ~RHITexture() override {}
};
using RHITexturePtr = RHIObjectPtr<RHITexture>;

// Views
class RHIUniformBufferView : public RHIObject
{
public:
    virtual ~RHIUniformBufferView() override {}
};
using RHIUniformBufferViewPtr = RHIObjectPtr<RHIUniformBufferView>;

class RHITextureROView : public RHIObject
{
public:
    virtual ~RHITextureROView() override {}
};
using RHITextureROViewPtr = RHIObjectPtr<RHITextureROView>;

class RHITextureRWView : public RHIObject
{
public:
    virtual ~RHITextureRWView() override {}
};
using RHITextureRWViewPtr = RHIObjectPtr<RHITextureRWView>;

class RHIRenderTargetView : public RHIObject
{
public:
    virtual ~RHIRenderTargetView() override {}

    virtual glm::uvec3 GetSize() const { return glm::uvec3( 0, 0, 0 ); }
};
using RHIRenderTargetViewPtr = RHIObjectPtr<RHIRenderTargetView>;

class RHISampler : public RHIObject
{
public:
    virtual ~RHISampler() override {}
};
using RHISamplerPtr = RHIObjectPtr<RHISampler>;

class RHIAccelerationStructure : public RHIObject
{
public:
    virtual size_t GetSize() const { NOTIMPL; return 0; }
    virtual ~RHIAccelerationStructure() override {}
};
using RHIAccelerationStructurePtr = RHIObjectPtr<RHIAccelerationStructure>;

template<typename T>
using UniquePtrWithDeleter = std::unique_ptr<T, void( * )( T* )>;

using RHIPtr = UniquePtrWithDeleter<RHI>;
