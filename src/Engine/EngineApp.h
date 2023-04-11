#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

struct CommandLineArguments
{
	std::string engine_content_path;
	std::string log_path;
};

class EngineApp
{
private:

	glm::ivec2 m_window_size = glm::ivec2( 800, 600 );

	struct SDL_Window* m_main_wnd = nullptr;

	std::unique_ptr<struct SDLVulkanWindowInterface> m_window_iface = nullptr;

	// synchronization
	std::vector<RHIObjectPtr<RHISemaphore>> m_image_available_semaphores;
	std::vector<RHIObjectPtr<RHISemaphore>> m_render_finished_semaphores;
	std::vector<RHIFence> m_inflight_fences;
	std::vector<uint64_t> m_imgui_frames;
	uint32_t m_current_frame = 0;

	bool m_fb_resized = false;

	std::unique_ptr<class ImguiBackend> m_imgui;

protected:
	RHIObjectPtr<RHISwapChain> m_swapchain = nullptr;
	static const size_t m_max_frames_in_flight = 2;
	RHIPtr m_rhi;
	CommandLineArguments m_cmd_line_args;

public:
	EngineApp();
	~EngineApp();

	void Run( int argc, char** argv );

protected:
	virtual void ParseCommandLineDerived( int argc, char** argv ) {}

	virtual const char* GetMainWindowName() const { return "SnowEngine"; }
	virtual const char* GetAppName() const { return "SnowEngine"; }

	virtual void OnInit() {}
	virtual void OnCleanup() {}
	virtual void OnUpdate() {}
	virtual void OnDrawFrame( std::vector<RHICommandList*>& lists_to_submit ) {}

	// returns frame index % m_max_frames_in_flight
	uint32_t GetCurrentFrameIdx() const { return m_current_frame; }

private:
	void InitCoreGlobals();
	void ParseCommandLine( int argc, char** argv );

	void InitRHI();
	void MainLoop();
	void Cleanup();

	std::vector<const char*> GetSDLExtensions() const;

	void Update();

	void DrawFrame();

	void CreateSyncObjects();

	void InitImGUI();
};