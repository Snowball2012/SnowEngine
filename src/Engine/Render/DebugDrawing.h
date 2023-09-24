#pragma once

#include "../StdAfx.h"

class DebugDrawingInitProgram;
class RGBuffer;
class RGPass;
struct SceneViewFrameData;

struct DebugDrawingSceneViewData
{
    RHIBufferPtr m_lines_buf = nullptr;
    RHIBufferPtr m_lines_indirect_args_buf = nullptr;
};

struct DebugDrawingContext
{
    RGPass* init_pass = nullptr;
    RGPass* finalize_collection_pass = nullptr;
    RGPass* draw_pass = nullptr;

    RGBuffer* lines_buf = nullptr;
    RGBuffer* lines_args_buf = nullptr;
};

class DebugDrawing
{
private:
    std::unique_ptr<DebugDrawingInitProgram> m_program_init = nullptr;

public:
    DebugDrawing( RHIDescriptorSetLayout* scene_view_dsl );
    ~DebugDrawing();

    bool InitSceneViewData( DebugDrawingSceneViewData& data ) const;

    void AddInitPasses( SceneViewFrameData& data, DebugDrawingContext& ctx ) const;
    void AddDrawPasses( SceneViewFrameData& data, DebugDrawingContext& ctx ) const;

    void RecordInitPasses( RHICommandList& cmd_list, const SceneViewFrameData& data, const DebugDrawingContext& ctx ) const;
    void RecordDrawPasses( RHICommandList& cmd_list, const SceneViewFrameData& data, const DebugDrawingContext& ctx ) const;
};