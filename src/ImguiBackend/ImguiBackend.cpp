#include "ImguiBackend.h"

#include <Core/Core.h>

#include <RHI/RHI.h>

#include <imgui/imgui.h>
#include "imgui_impl_sdl2.h"

#include <SDL2/SDL_events.h>


ImguiBackend::ImguiBackend( SDL_Window* window, RHI* rhi, RHIFormat target_format )
    : m_rhi( rhi )
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); ( void )io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForSDLRenderer( window, nullptr );

    SetupPSO( target_format );

    // has to be set up somewhere before NewFrame, or ImGui will crash
    SetupFonts();

    NewFrame();
}

ImguiBackend::~ImguiBackend()
{
    ImGui_ImplSDL2_Shutdown();

    ImGui::DestroyContext();
}

ImguiProcessEventResult ImguiBackend::ProcessEvent( SDL_Event* event )
{
    ImGui_ImplSDL2_ProcessEvent( event );

    ImguiProcessEventResult res;
    res.wantsCaptureKeyboard = ImGui::GetIO().WantCaptureKeyboard;
    res.wantsCaptureMouse = ImGui::GetIO().WantCaptureKeyboard;

    return res;
}

void ImguiBackend::NewFrame()
{
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    m_cur_frame++;
}

ImguiRenderResult ImguiBackend::RenderFrame( RHIRenderTargetView& rtv )
{
    ImguiRenderResult res = {};

    res.frame_idx = m_cur_frame;

    ImGui::Render();

    BOOST_SCOPE_EXIT( this_ )
    {
        this_->NewFrame();
    } BOOST_SCOPE_EXIT_END

    ImDrawData* draw_data = ImGui::GetDrawData();

    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    int fb_width = ( int )( draw_data->DisplaySize.x * draw_data->FramebufferScale.x );
    int fb_height = ( int )( draw_data->DisplaySize.y * draw_data->FramebufferScale.y );

    // Sometimes when we are resizing a swapchain imgui can have previous display size. Restrict rendering to the true window size in that case
    glm::uvec3 rt_size = rtv.GetSize();
    fb_width = std::min<int>( fb_width, rt_size.x );
    fb_height = std::min<int>( fb_height, rt_size.y );

    if ( fb_width <= 0 || fb_height <= 0 )
        return res;

    if ( draw_data->CmdListsCount <= 0 )
        return res;

    RHIUploadBuffer* vtx_buf = nullptr;
    RHIUploadBuffer* idx_buf = nullptr;
    if ( draw_data->TotalVtxCount > 0 )
    {
        const size_t vertex_size = draw_data->TotalVtxCount * sizeof( ImDrawVert );
        const size_t index_size = draw_data->TotalIdxCount * sizeof( ImDrawIdx );

        FrameData* cache = FindFittingCache( vertex_size, index_size );

        if ( !cache )
            return res; // should never happen

        vtx_buf = cache->vertices.get();
        idx_buf = cache->indices.get();

        cache->submitted_frame_idx = m_cur_frame;

        size_t cur_offset_vtx = 0;
        size_t cur_offset_idx = 0;
        for ( int n = 0; n < draw_data->CmdListsCount; n++ )
        {
            const ImDrawList* cmd_list = draw_data->CmdLists[n];
            vtx_buf->WriteBytes( cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof( ImDrawVert ), cur_offset_vtx );
            idx_buf->WriteBytes( cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof( ImDrawIdx ), cur_offset_idx );
            cur_offset_vtx += cmd_list->VtxBuffer.Size * sizeof( ImDrawVert );
            cur_offset_idx += cmd_list->IdxBuffer.Size * sizeof( ImDrawIdx );
        }
    }

    res.cl = m_rhi->GetCommandList( RHI::QueueType::Graphics );
    if ( !res.cl )
        return res;

    RecordCommandList(
        draw_data, fb_width, fb_height,
        vtx_buf ? vtx_buf->GetBuffer() : nullptr,
        idx_buf ? idx_buf->GetBuffer() : nullptr,
        *res.cl, rtv );

    return res;
}

void ImguiBackend::MarkFrameAsCompleted( uint64_t frame_idx )
{
    m_submitted_frame = std::max( m_submitted_frame, frame_idx );
}

void ImguiBackend::SetupFonts()
{
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontDefault();
    io.Fonts->Build();

    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32( &pixels, &width, &height );

    VERIFY_NOT_EQUAL( pixels, nullptr );

    uint64_t image_size = sizeof( uint8_t ) * 4 * width * height;

    RHI::BufferInfo staging_info = {};

    staging_info.size = image_size;
    staging_info.usage = RHIBufferUsageFlags::TransferSrc;

    RHIUploadBufferPtr staging_buf = m_rhi->CreateUploadBuffer( staging_info );

    VERIFY_NOT_EQUAL( staging_buf, nullptr );

    staging_buf->WriteBytes( pixels, staging_info.size, 0 );

    RHI::TextureInfo tex_info = {};
    tex_info.dimensions = RHITextureDimensions::T2D;
    tex_info.width = width;
    tex_info.height = height;
    tex_info.depth = 1;
    tex_info.mips = 1;
    tex_info.array_layers = 1;
    tex_info.format = RHIFormat::R8G8B8A8_UNORM;
    tex_info.usage = RHITextureUsageFlags::TextureROView | RHITextureUsageFlags::TransferDst;
    tex_info.initial_layout = RHITextureLayout::TransferDst;

    m_fonts_atlas = m_rhi->CreateTexture( tex_info );

    RHI::TextureROViewInfo srv_info = {};
    srv_info.texture = m_fonts_atlas.get();
    m_fonts_atlas_srv = m_rhi->CreateTextureROView( srv_info );

    RHICommandList* upload_cl = m_rhi->GetCommandList( RHI::QueueType::Graphics );

    upload_cl->Begin();

    RHIBufferTextureCopyRegion region = {};
    region.texture_subresource.mip_count = 1;
    region.texture_subresource.array_count = 1;
    region.texture_extent[0] = width;
    region.texture_extent[1] = height;
    region.texture_extent[2] = 1;

    upload_cl->CopyBufferToTexture( *staging_buf->GetBuffer(), *m_fonts_atlas, &region, 1);

    RHITextureBarrier barrier = {};
    barrier.layout_src = RHITextureLayout::TransferDst;
    barrier.layout_dst = RHITextureLayout::ShaderReadOnly;
    barrier.texture = m_fonts_atlas.get();
    barrier.subresources.mip_count = 1;
    barrier.subresources.array_count = 1;

    upload_cl->TextureBarriers( &barrier, 1 );

    upload_cl->End();

    RHI::SubmitInfo submit_info = {};
    submit_info.wait_semaphore_count = 0;
    submit_info.cmd_list_count = 1;
    RHICommandList* lists[] = { upload_cl };
    submit_info.cmd_lists = lists;

    RHIFence completion_fence = m_rhi->SubmitCommandLists( submit_info );

    m_rhi->WaitForFenceCompletion( completion_fence );
    
    RHI::SamplerInfo sampler_info = {};
    m_fonts_sampler = m_rhi->CreateSampler( sampler_info );

    // SetupPSO has to be called before this
    VERIFY_NOT_EQUAL( m_desc_set, nullptr );
    
    m_desc_set->BindTextureROView( 0, 0, *m_fonts_atlas_srv );
    m_desc_set->BindSampler( 1, 0, *m_fonts_sampler );
    m_desc_set->FlushBinds();

    io.Fonts->SetTexID( ( ImTextureID )m_fonts_atlas_srv.get() );
}

struct ImguiPushConstants
{
    glm::vec2 scale;
    glm::vec2 translate;
};

void ImguiBackend::SetupPSO( RHIFormat target_format )
{
    RHIGraphicsPipelineInfo pso_info = {};

    pso_info.rts_count = 1;
    RHIPipelineRTInfo rt_info = {};
    rt_info.format = target_format;
    rt_info.enable_blend = true;
    rt_info.src_alpha_blend = RHIBlendFactor::One;
    rt_info.dst_alpha_blend = RHIBlendFactor::OneMinusSrcAlpha;
    rt_info.src_color_blend = RHIBlendFactor::One;
    rt_info.dst_color_blend = RHIBlendFactor::OneMinusSrcAlpha;
    pso_info.rt_info = &rt_info;

    RHI::ShaderCreateInfo vs_info = {};

    std::wstring shader_path = ToWString( ToOSPath( "#engine/shaders/imgui.hlsl" ).c_str() );

    vs_info.filename = shader_path.c_str();
    vs_info.frequency = RHI::ShaderFrequency::Vertex;
    vs_info.entry_point = "MainVS";
    RHIShader* vs = m_rhi->CreateShader( vs_info );

    RHI::ShaderCreateInfo ps_info = {};
    ps_info.filename = shader_path.c_str();
    ps_info.frequency = RHI::ShaderFrequency::Pixel;
    ps_info.entry_point = "MainPS";
    RHIShader* ps = m_rhi->CreateShader( ps_info );

    pso_info.vs = vs;
    pso_info.ps = ps;

    RHIPrimitiveAttributeInfo attributes[] =
    {
        { "POSITION", RHIFormat::R32G32_SFLOAT, offsetof( ImDrawVert, pos ) },
        { "TEXCOORD0", RHIFormat::R32G32_SFLOAT, offsetof( ImDrawVert, uv ) },
        { "TEXCOORD1", RHIFormat::R8G8B8A8_UNORM, offsetof( ImDrawVert, col ) }
    };

    RHIPrimitiveBufferLayout pb_layout = { attributes, _countof( attributes ), sizeof( ImDrawVert ) };

    const RHIPrimitiveBufferLayout* layouts = { &pb_layout };

    RHIInputAssemblerInfo ia_info = {};
    ia_info.buffers_count = 1;
    ia_info.primitive_buffers = &layouts;
    RHIPrimitiveFrequency frequencies[] = {RHIPrimitiveFrequency::PerVertex};
    ia_info.frequencies = frequencies;

    pso_info.input_assembler = &ia_info;

    RHI::DescriptorViewRange view_ranges[] =
    {
        { RHIShaderBindingType::TextureRO, 1, RHIShaderStageFlags::PixelShader },
        { RHIShaderBindingType::Sampler, 1, RHIShaderStageFlags::PixelShader },
    };

    RHI::DescriptorSetLayoutInfo dsl_info = {};
    dsl_info.ranges = view_ranges;
    dsl_info.range_count = _countof( view_ranges );

    RHIDescriptorSetLayoutPtr sbtl = m_rhi->CreateDescriptorSetLayout( dsl_info );

    // This is supposed to be the call that creates m_desc_set for the first time. Otherwise we may need to rebind fonts
    SE_ENSURE( m_desc_set == nullptr );
    m_desc_set = m_rhi->CreateDescriptorSet( *sbtl );

    RHIDescriptorSetLayout* pipeline_dsls[] = { sbtl.get() };
    
    RHI::ShaderBindingLayoutInfo sbl_info = {};
    sbl_info.push_constants_size = sizeof( ImguiPushConstants );
    sbl_info.tables = pipeline_dsls;
    sbl_info.table_count = _countof( pipeline_dsls );
    RHIShaderBindingLayoutPtr sbl = m_rhi->CreateShaderBindingLayout( sbl_info );

    pso_info.binding_layout = sbl.get();

    pso_info.rasterizer.cull_mode = RHICullModeFlags::None;

    m_pso = m_rhi->CreatePSO( pso_info );

    VERIFY_NOT_EQUAL( m_pso, nullptr );
}

void ImguiBackend::RecordCommandList(
    ImDrawData* draw_data,
    uint32_t fb_width, uint32_t fb_height,
    RHIBuffer* vtx_buf, RHIBuffer* idx_buf,
    RHICommandList& cl, RHIRenderTargetView& rtv ) const
{
    cl.Begin();

    RHIPassRTVInfo rt = {};
    rt.load_op = RHILoadOp::PreserveContents;
    rt.store_op = RHIStoreOp::Store;
    rt.rtv = &rtv;
    rt.clear_value.float32[3] = 1.0f;

    RHIPassInfo pass_info = {};
    pass_info.render_area = RHIRect2D{ .offset = glm::ivec2( 0,0 ), .extent = glm::uvec2( fb_width, fb_height ) };
    pass_info.render_targets = &rt;
    pass_info.render_targets_count = 1;
    cl.BeginPass( pass_info );

    cl.SetPSO( *m_pso );

    if ( vtx_buf )
        cl.SetVertexBuffers( 0, vtx_buf, 1, nullptr );

    if ( idx_buf )
        cl.SetIndexBuffer(
            *idx_buf,
            sizeof( ImDrawIdx ) == 2 ? RHIIndexBufferType::UInt16 : RHIIndexBufferType::UInt32,
            0 );

    cl.BindDescriptorSet( 0, *m_desc_set );

    RHIViewport viewports[] =
    {
        {.width = float( fb_width ), .height = float( fb_height ), .max_depth = 1.0f },
    };

    cl.SetViewports( 0, viewports, _countof( viewports ) );

    ImguiPushConstants pc = {};
    pc.scale = glm::vec2(
        2.0f / draw_data->DisplaySize.x,
        2.0f / draw_data->DisplaySize.y );
    pc.translate = glm::vec2(
        -1.0f - draw_data->DisplayPos.x * pc.scale.x,
        -1.0f - draw_data->DisplayPos.y * pc.scale.y );

    cl.PushConstants( 0, &pc, sizeof( pc ) );


    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // Render command lists
    // (Because we merged all buffers into a single one, we maintain our own offset into them)
    int global_vtx_offset = 0;
    int global_idx_offset = 0;
    for ( int n = 0; n < draw_data->CmdListsCount; n++ )
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        for ( int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++ )
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if ( pcmd->UserCallback != nullptr )
                NOTIMPL;

            // Project scissor/clipping rectangles into framebuffer space
            ImVec2 clip_min( ( pcmd->ClipRect.x - clip_off.x ) * clip_scale.x, ( pcmd->ClipRect.y - clip_off.y ) * clip_scale.y );
            ImVec2 clip_max( ( pcmd->ClipRect.z - clip_off.x ) * clip_scale.x, ( pcmd->ClipRect.w - clip_off.y ) * clip_scale.y );

            // Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
            if ( clip_min.x < 0.0f ) { clip_min.x = 0.0f; }
            if ( clip_min.y < 0.0f ) { clip_min.y = 0.0f; }
            if ( clip_max.x > fb_width ) { clip_max.x = ( float )fb_width; }
            if ( clip_max.y > fb_height ) { clip_max.y = ( float )fb_height; }
            if ( clip_max.x <= clip_min.x || clip_max.y <= clip_min.y )
                continue;

            // Apply scissor/clipping rectangle
            RHIRect2D scissor;
            scissor.offset.x = ( int32_t )( clip_min.x );
            scissor.offset.y = ( int32_t )( clip_min.y );
            scissor.extent.x = ( uint32_t )( clip_max.x - clip_min.x );
            scissor.extent.y = ( uint32_t )( clip_max.y - clip_min.y );
            cl.SetScissors( 0, &scissor, 1 );

            // Bind DescriptorSet with font or user texture
            RHITextureROView* texture_srv = ( RHITextureROView* )pcmd->TextureId;

            if ( texture_srv && texture_srv != m_fonts_atlas_srv )
                NOTIMPL; // don't support user textures for now

            // Draw
            cl.DrawIndexed( pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0 );
        }

        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }

    cl.EndPass();

    cl.End();
}

ImguiBackend::FrameData* ImguiBackend::FindFittingCache( size_t vertex_buf_size, size_t index_buf_size )
{
    // Sanity check. Large cache size most likely means MarkFrameAsCompleted is not called correctly, causing memory leak
    SE_ENSURE( m_cache.size() < 16 );

    auto found_cache = std::ranges::find_if( m_cache, [&]( const FrameData& v )
        {
            if ( v.submitted_frame_idx > m_submitted_frame )
                return false;

            if ( v.vertices->GetBuffer()->GetSize() < vertex_buf_size )
                return false;

            if ( v.indices->GetBuffer()->GetSize() < index_buf_size )
                return false;

            return true;
        } );

    if ( found_cache == m_cache.end() )
    {
        found_cache = std::ranges::find_if( m_cache, [&]( const FrameData& v )
            {
                if ( v.submitted_frame_idx > m_submitted_frame )
                    return false;

                return true;
            } );

        if ( found_cache == m_cache.end() )
        {
            m_cache.emplace_back();

            found_cache = std::prev( m_cache.end() );
        }
    }

    if ( !SE_ENSURE( found_cache != m_cache.end() ) )
        return nullptr;

    auto createOrResizeBufferToFit = [&]( RHIUploadBufferPtr& buf, size_t size, RHIBufferUsageFlags usage )
    {
        if ( !buf || buf->GetBuffer()->GetSize() < size )
        {
            RHI::BufferInfo buf_info = {};
            buf_info.size = size;
            buf_info.usage = usage;

            buf = m_rhi->CreateUploadBuffer( buf_info );
        }
    };

    createOrResizeBufferToFit( found_cache->vertices, vertex_buf_size, RHIBufferUsageFlags::VertexBuffer );
    createOrResizeBufferToFit( found_cache->indices, index_buf_size, RHIBufferUsageFlags::IndexBuffer );

    return &(*found_cache);
}
