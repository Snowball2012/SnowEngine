#include "ImguiBackend.h"

#include <Core/Core.h>

#include <imgui/imgui.h>
#include "imgui_impl_sdl2.h"

ImguiBackend::ImguiBackend( SDL_Window* window )
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
}

ImguiBackend::~ImguiBackend()
{
    ImGui_ImplSDL2_Shutdown();

    ImGui::DestroyContext();
}

void ImguiBackend::InitRHI()
{
    NOTIMPL;
}
