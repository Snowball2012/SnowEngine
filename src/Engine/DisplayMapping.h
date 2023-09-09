#pragma once

#include "StdAfx.h"

#include "Assets.h"

struct SceneViewFrameData;

class DisplayMappingProgram;
class RGPass;
class RGTexture;

struct DisplayMappingContext
{
    RGPass* dbg_copy_pass = nullptr;
    RGPass* main_pass = nullptr;
    const RGTexture* input_tex = nullptr;
    const RGTexture* output_tex = nullptr;
};

class DisplayMapping
{
private:
    TextureAssetPtr m_dbg_texture = nullptr;
    std::unique_ptr<DisplayMappingProgram> m_program = nullptr;

public:
    DisplayMapping();
    ~DisplayMapping();

    void SetupRendergraph( SceneViewFrameData& data, DisplayMappingContext& ctx ) const;

    void DisplayMappingPass( RHICommandList& cmd_list, const SceneViewFrameData& data, const DisplayMappingContext& ctx ) const;

    void DebugUI();
};