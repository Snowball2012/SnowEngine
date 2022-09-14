#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

#include "RenderResources.h"

class D3D12TestApp
{
private:
	static const size_t m_max_frames_in_flight = 2;

	glm::ivec2 m_window_size = glm::ivec2(800, 600);

	struct SDL_Window* m_main_wnd = nullptr;

	std::vector<RHIObjectPtr<RHISemaphore>> m_image_available_semaphores;
	std::vector<RHIObjectPtr<RHISemaphore>> m_render_finished_semaphores;
	std::vector<RHIFence> m_inflight_fences;
	uint32_t m_current_frame = 0;

	bool m_fb_resized = false;

	RHIPtr m_rhi;

public:
	D3D12TestApp();
	~D3D12TestApp();

	void Run();

private:
	void InitRHI();
	void MainLoop();
	void Cleanup();

	void DrawFrame();

	void CreateSyncObjects();
};