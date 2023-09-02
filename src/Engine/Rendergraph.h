#pragma once


#include "StdAfx.h"


SE_LOG_CATEGORY( Rendergraph );


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


class RGTexture : public RGResource
{
    bool m_is_external = false;

    const RHITexture* m_rhi_texture = nullptr;

public:
    RGTexture( uint64_t handle, const char* name, bool is_external );

    bool IsExternal() const { return m_is_external; }

    void SetRHITexture( const RHITexture* tex ) { m_rhi_texture = tex; }

    const RHITexture* GetRHITexture() const { return m_rhi_texture; }
};


enum class RGTextureUsage
{
    Undefined = 0,
    ShaderRead,
    ShaderReadWrite,
    RenderTarget
};


struct RGPassTexture
{
    const RGTexture* texture = nullptr;
    RGTextureUsage usage = RGTextureUsage::Undefined;
};


class RGPass
{
    friend class Rendergraph;

    RHI::QueueType m_queue_type = RHI::QueueType::Count;

    std::unordered_map<uint64_t, RGPassTexture> m_used_textures;

    std::vector<RHICommandList*> m_cmd_lists;

    std::vector<RHITextureBarrier> m_pass_start_texture_barriers;

    std::string m_name;

    bool m_borrowed_list = false;

public:
    RGPass( RHI::QueueType queue_type, const char* name );

    bool UseTexture( const RGTexture& texture, RGTextureUsage usage );

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

    std::vector<RHITextureBarrier> m_final_barriers;

public:
    RGPass* AddPass( RHI::QueueType queue_type, const char* name );

    RGExternalTexture* RegisterExternalTexture( const RGExternalTextureDesc& desc );
    RGResource* CreateTransientTexture();
    RGResource* RegisterExternalAS();
    RGResource* CreateTransientBuffer();

    bool Compile();

    RHIFence Submit( const RGSubmitInfo& info );

private:
    uint64_t GenerateHandle() { return m_handle_generator++; }
};