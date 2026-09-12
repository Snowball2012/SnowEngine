#pragma once

#include "StdAfx.h"

#include "Editor.h"
#include "MaterialEditor.h"

#include <Engine/EngineApp.h>
#include <Engine/Assets.h>
#include <Engine/RHIUtils.h>
#include <Engine/Scene.h>
#include <Engine/LevelObjects.h>
#include <ImguiBackend/ImguiBackend.h>


class SandboxApp : public EngineApp
{
private:
    struct ScreenshotRequest
    {
        bool active = false;
        bool include_imgui = false;
        std::string output_path;
    };

    struct ScreenshotReadback
    {
        bool active = false;
        bool include_imgui = false;
        glm::uvec2 extent = {};
        RHIFormat format = RHIFormat::Undefined;
        RHIReadbackBufferPtr buffer;
        std::string output_path;
    };

    // GUI state
    bool m_show_imgui_demo = false;
    bool m_show_world_outliner = false;

    std::unique_ptr<LevelEditor> m_editor;
    std::unique_ptr<MaterialEditor> m_material_editor;

    decltype( std::chrono::high_resolution_clock::now() ) m_last_tick_time;

    ScreenshotRequest m_screenshot_request;
    std::array<ScreenshotReadback, m_max_frames_in_flight> m_screenshot_readbacks;
    uint64_t m_screenshot_sequence = 0;

public:
    SandboxApp();
    ~SandboxApp();

    bool QueueScreenshot( bool include_imgui, std::string_view output_path, std::string& output );

private:
    virtual void OnInit() override;

    virtual void OnCleanup() override;

    virtual void OnDrawFrame( Rendergraph& framegraph, const AppGPUReadbackData& readback_data, RHICommandList* ui_cmd_list ) override;

    virtual void OnUpdate() override;

    virtual void OnSwapChainRecreated() override;

    virtual void OnFrameRenderFinish( const AppGPUReadbackData& data ) override;
    virtual void CreateReadbackData( AppGPUReadbackData& data ) override;

    virtual const char* GetMainWindowName() const override { return "SnowEngine Sandbox"; }
    virtual const char* GetAppName() const override { return "SnowEngineSandbox"; }

    ScreenshotReadback* BeginScreenshotReadback();
    void CompleteScreenshotReadback( ScreenshotReadback& screenshot );
    void UpdateGui();
};
