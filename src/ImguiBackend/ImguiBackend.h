#pragma once

#include <RHI/RHI.h>

struct ImguiProcessEventResult
{
	bool wantsCaptureKeyboard = false;
	bool wantsCaptureMouse = false;
};

class ImguiBackend
{
private:
	class RHI* m_rhi = nullptr;

	RHITexturePtr m_fonts_atlas = nullptr;
	RHITextureSRVPtr m_fonts_atlas_srv = nullptr;

public:
	ImguiBackend( struct SDL_Window* window, RHI* rhi );
	~ImguiBackend();

	ImguiProcessEventResult ProcessEvent( union SDL_Event* event );

	class RHICommandList* RenderFrame();

private:
	void NewFrame();
	void InitRHI();
	void SetupFonts();
};