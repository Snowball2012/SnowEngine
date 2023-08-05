#pragma once


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
public:
    RGTexture( uint64_t handle, const char* name );
};


enum class RGTextureUsage
{
    ShaderRead = 0,
    ShaderReadWrite,
    RenderTarget
};


struct RGPassTexture
{
    RGTexture* texture = nullptr;
    RGTextureUsage usage = RGTextureUsage::ShaderRead;
};


class RGPass
{
    friend class Rendergraph;

    RHI::QueueType m_queue_type = RHI::QueueType::Count;

    std::unordered_map<uint64_t, RGPassTexture> m_used_textures;

    std::vector<RHICommandList*> m_cmd_lists;

    std::string m_name;

public:
    RGPass( RHI::QueueType queue_type, const char* name );

    bool UseTexture( RGTexture& texture, RGTextureUsage usage );

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
    void BorrowCommandList( RHICommandList& cmd_list ) { NOTIMPL; }
};


struct RendergraphSubmission
{
    std::vector<RGPass*> passes;
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
};


class Rendergraph
{
private:
    uint64_t m_handle_generator = 0;

    std::vector<std::unique_ptr<RGPass>> m_passes;

    std::vector<RendergraphSubmission> m_submissions;

    std::vector<std::unique_ptr<RGExternalTexture>> m_external_textures;

public:
    RGPass* AddPass( RHI::QueueType queue_type, const char* name );

    RGExternalTexture* RegisterExternalTexture( const RGExternalTextureDesc& desc );
    RGResource* CreateTransientTexture();
    RGResource* RegisterExternalAS();
    RGResource* CreateTransientBuffer();

    bool Compile();

    bool Submit();

private:
    uint64_t GenerateHandle() { return m_handle_generator++; }
};