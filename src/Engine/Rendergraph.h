#pragma once


#include "StdAfx.h"

#include "UploadBufferPool.h"

SE_LOG_CATEGORY( Rendergraph );


class DescriptorSetPool;
class RHICommandList;


class RGResource
{
private:
    uint64_t m_handle = -1;

    std::string m_name;

public:
    RGResource( uint64_t handle, const char* name ) : m_handle( handle ), m_name( name ) {}

    uint64_t GetHandle() const { return m_handle; }

    const std::string& GetName() const { return m_name; }
};

class RGTexture;

enum class RGTextureUsage
{
    Undefined = 0,
    ShaderRead,
    ShaderReadWrite,
    RenderTarget
};

class RGTextureROView
{
private:
    friend class RGTexture;
    const RGTexture* m_texture = nullptr;
    RHITextureROView* m_rhi_view = nullptr;

public:

    const RGTexture* GetTexture() const { return m_texture; }
    RHITextureROView* GetRHIView() const { return m_rhi_view; }
};

class RGTextureRWView
{
private:
    friend class RGTexture;
    const RGTexture* m_texture = nullptr;
    RHITextureRWView* m_rhi_view = nullptr;

public:
    const RGTexture* GetTexture() const { return m_texture; }
    RHITextureRWView* GetRHIView() const { return m_rhi_view; }
};

class RGRenderTargetView
{
private:
    friend class RGTexture;
    const RGTexture* m_texture = nullptr;
    RHIRenderTargetView* m_rhi_view = nullptr;

public:
    const RGTexture* GetTexture() const { return m_texture; }
    RHIRenderTargetView* GetRHIView() const { return m_rhi_view; }
};

struct RGExternalTextureROViewDesc
{
    RHITextureROView* rhi_view = nullptr;
};

struct RGExternalTextureRWViewDesc
{
    RHITextureRWView* rhi_view = nullptr;
};

struct RGExternalRenderTargetViewDesc
{
    RHIRenderTargetView* rhi_view = nullptr;
};

class RGTexture : public RGResource
{
    bool m_is_external = false;

    const RHITexture* m_rhi_texture = nullptr;

    // @todo: allow multiple views of the same type
    std::optional<RGTextureROView> m_ro_view;
    std::optional<RGTextureRWView> m_rw_view;
    std::optional<RGRenderTargetView> m_rt_view;

public:
    RGTexture( uint64_t handle, const char* name, bool is_external );

    bool IsExternal() const { return m_is_external; }

    void SetRHITexture( const RHITexture* tex ) { m_rhi_texture = tex; }
    const RHITexture* GetRHITexture() const { return m_rhi_texture; }

    const RGTextureROView* RegisterExternalROView( const RGExternalTextureROViewDesc& desc );
    const RGTextureRWView* RegisterExternalRWView( const RGExternalTextureRWViewDesc& desc );
    const RGRenderTargetView* RegisterExternalRTView( const RGExternalRenderTargetViewDesc& desc );
    const RGTextureROView* GetROView() const { return m_ro_view.has_value() ? &*m_ro_view : nullptr; }
    const RGTextureRWView* GetRWView() const { return m_rw_view.has_value() ? &*m_rw_view : nullptr; }
    const RGRenderTargetView* GetRTView() const { return m_rt_view.has_value() ? &*m_rt_view : nullptr; }
};


struct RGPassTexture
{
    const RGTexture* texture = nullptr;
    RGTextureUsage usage = RGTextureUsage::Undefined;
};




class RGBuffer;

enum class RGBufferUsage
{
    Undefined = 0,
    ShaderRead,
    ShaderReadWrite,
    ShaderWriteOnly,
    IndirectArgs,
};

class RGBuffer : public RGResource
{
    RHIBuffer* m_rhi_buffer = nullptr;

public:
    RGBuffer( uint64_t handle, const char* name );

    void SetRHIBuffer( RHIBuffer* buf ) { m_rhi_buffer = buf; }
    RHIBuffer* GetRHIBuffer() const { return m_rhi_buffer; }
};

struct RGPassBuffer
{
    const RGBuffer* buffer = nullptr;
    RGBufferUsage usage = RGBufferUsage::Undefined;
};



class RGPass
{
    friend class Rendergraph;

    RHI::QueueType m_queue_type = RHI::QueueType::Count;

    std::unordered_map<uint64_t, RGPassTexture> m_used_textures;

    std::unordered_map<uint64_t, RGPassBuffer> m_used_buffers;

    std::vector<RHICommandList*> m_cmd_lists;

    std::vector<RHITextureBarrier> m_pass_start_texture_barriers;

    std::string m_name;

    bool m_borrowed_list = false;

public:
    RGPass( RHI::QueueType queue_type, const char* name );

    bool UseTexture( const RGTexture& texture, RGTextureUsage usage );
    bool UseTextureView( const RGRenderTargetView& view );
    bool UseTextureView( const RGTextureROView& view );
    bool UseTextureView( const RGTextureRWView& view );
    bool UseBuffer( const RGBuffer& buffer, RGBufferUsage usage );

    void AddCommandList( RHICommandList& cmd_list );

    // Has to be called, when all command lists are written for that pass
    void EndPass();

    // If true, you can reuse command list from previous pass. You can only borrow one last command list, and you still have to call EndPass at the end of the pass
    // This allows to avoid having a separate command list for short consequent passes.
    // It can be used like this:
    // prev_pass->AddCommandList( list );
    // prev_pass->EndPass();
    // curr_pass->BorrowCommmandList( list );
    // curr_pass->AddCommandList( some_other_list ); // optional
    // curr_pass->EndPass();
    bool CanBorrowCommandListFromPreviousPass() const { NOTIMPL; return false; }
    bool BorrowCommandList( RHICommandList& cmd_list );

private:
    void AddPassBeginCommands( RHICommandList& cmd_list );
};


struct RendergraphSubmission
{
    std::vector<RGPass*> passes;
    std::vector<RHITextureBarrier> end_barriers;
    RHI::QueueType type = RHI::QueueType::Graphics;
};


struct RGExternalTextureDesc
{
    const char* name = nullptr;
    const RHITexture* rhi_texture = nullptr;
    RHITextureLayout initial_layout = RHITextureLayout::Undefined;
    RHITextureLayout final_layout = RHITextureLayout::Undefined;
};


class RGExternalTexture : public RGTexture
{
private:
    RGExternalTextureDesc m_desc = {};

public:
    RGExternalTexture( uint64_t handle, const RGExternalTextureDesc& desc );

    const RGExternalTextureDesc& GetDesc() const { return m_desc; }
};

struct RGExternalTextureEntry
{
    std::unique_ptr<RGExternalTexture> texture = nullptr;
    RGTextureUsage first_usage = RGTextureUsage::Undefined;
    RGTextureUsage last_usage = RGTextureUsage::Undefined;
};

struct RGExternalBufferDesc
{
    const char* name = nullptr;
    RHIBuffer* rhi_buffer = nullptr;
};

class RGExternalBuffer : public RGBuffer
{
private:
    RGExternalBufferDesc m_desc = {};

public:
    RGExternalBuffer( uint64_t handle, const RGExternalBufferDesc& desc );

    const RGExternalBufferDesc& GetDesc() const { return m_desc; }
};

struct RGExternalBufferEntry
{
    std::unique_ptr<RGExternalBuffer> buffer = nullptr;
};

struct RGSubmitInfo
{
    size_t wait_semaphore_count = 0;
    class RHISemaphore* const* semaphores_to_wait = nullptr;
    const RHIPipelineStageFlags* stages_to_wait = nullptr;
    class RHISemaphore* semaphore_to_signal = nullptr;
};


class Rendergraph
{
private:
    uint64_t m_handle_generator = 0;

    std::vector<std::unique_ptr<RGPass>> m_passes;

    std::vector<RendergraphSubmission> m_submissions;

    std::unordered_map<uint64_t, RGExternalTextureEntry> m_external_textures;
    std::unordered_map<uint64_t, RGExternalBufferEntry> m_external_buffers;

    std::vector<RHITextureBarrier> m_final_barriers;

    std::unique_ptr<DescriptorSetPool> m_descriptors;
    std::unique_ptr<UploadBufferPool> m_upload_buffers_uniform;
    std::unique_ptr<UploadBufferPool> m_upload_buffers_structured;

public:
    Rendergraph();
    ~Rendergraph();

    RGPass* AddPass( RHI::QueueType queue_type, const char* name );

    RGExternalTexture* RegisterExternalTexture( const RGExternalTextureDesc& desc );
    RGExternalBuffer* RegisterExternalBuffer( const RGExternalBufferDesc& desc );
    RGResource* CreateTransientTexture();
    RGResource* RegisterExternalAS();
    RGResource* CreateTransientBuffer();

    bool Compile();

    RHIFence Submit( const RGSubmitInfo& info );

    void Reset();

    // returned descriptor may only be used until Submit() is called
    RHIDescriptorSet* AllocateFrameDescSet( RHIDescriptorSetLayout& layout );
    // returned buffer range may only be used until Submit() is called
    UploadBufferRange AllocateUploadBufferUniform( size_t size );
    UploadBufferRange AllocateUploadBufferStructured( size_t size );

    template<typename BufferType>
    UploadBufferRange AllocateUploadBufferUniform() { return AllocateUploadBufferUniform( sizeof( BufferType ) ); }

    // This may be problematic in DX12 where structured buffer element size is fixed on creation. Suballocation can be tricky
    template<typename BufferType>
    UploadBufferRange AllocateUploadBufferStructured( size_t num_elems ) { return AllocateUploadBufferStructured( sizeof( BufferType ) * num_elems ); }

private:
    uint64_t GenerateHandle() { return m_handle_generator++; }
};