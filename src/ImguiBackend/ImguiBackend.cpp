#include "ImguiBackend.h"

#include <Core/Core.h>

#include <RHI/RHI.h>

#include <imgui/imgui.h>
#include "imgui_impl_sdl2.h"

#include <SDL2/SDL_events.h>


ImguiBackend::ImguiBackend( SDL_Window* window, RHI* rhi )
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

    // Setup RHI backend
    InitRHI();

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
}

RHICommandList* ImguiBackend::RenderFrame()
{
    ImGui::Render();

    NewFrame();

    return nullptr;
}

void ImguiBackend::InitRHI()
{
    //NOTIMPL;
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
    tex_info.usage = RHITextureUsageFlags::SRV | RHITextureUsageFlags::TransferDst;
    tex_info.initial_layout = RHITextureLayout::TransferDst;

    m_fonts_atlas = m_rhi->CreateTexture( tex_info );

    RHI::TextureSRVInfo srv_info = {};
    srv_info.texture = m_fonts_atlas.get();
    m_fonts_atlas_srv = m_rhi->CreateSRV( srv_info );

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
}
