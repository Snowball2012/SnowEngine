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
public:
    // has to match hlsl
    static constexpr uint32_t DISPLAY_MAPPING_METHOD_LINEAR = 0;
    static constexpr uint32_t DISPLAY_MAPPING_METHOD_REINHARD = 1;
    static constexpr uint32_t DISPLAY_MAPPING_METHOD_REINHARD_WHITE_POINT = 2;
    static constexpr uint32_t DISPLAY_MAPPING_METHOD_REINHARD_LUMINANCE = 3;
    static constexpr uint32_t DISPLAY_MAPPING_METHOD_REINHARD_JODIE = 4;
    static constexpr uint32_t DISPLAY_MAPPING_METHOD_UNCHARTED2 = 5;
    static constexpr uint32_t DISPLAY_MAPPING_METHOD_ACES = 6;
    static constexpr uint32_t DISPLAY_MAPPING_METHOD_ACESCG = 7;
    static constexpr uint32_t DISPLAY_MAPPING_METHOD_AGX = 8;

    enum class Method : uint32_t
    {
        Linear = DISPLAY_MAPPING_METHOD_LINEAR,
        Reinhard = DISPLAY_MAPPING_METHOD_REINHARD,
        ReinhardWhitePoint = DISPLAY_MAPPING_METHOD_REINHARD_WHITE_POINT,
        ReinhardLuminance = DISPLAY_MAPPING_METHOD_REINHARD_LUMINANCE,
        ReinhardJodie = DISPLAY_MAPPING_METHOD_REINHARD_JODIE,
        Uncharted2 = DISPLAY_MAPPING_METHOD_UNCHARTED2,
        ACES = DISPLAY_MAPPING_METHOD_ACES,
        ACEScg = DISPLAY_MAPPING_METHOD_ACESCG,
        AgX = DISPLAY_MAPPING_METHOD_AGX
    };

private:
    TextureAssetPtr m_dbg_texture = nullptr;
    std::unique_ptr<DisplayMappingProgram> m_program = nullptr;

    float m_white_point = 1.0f;
    Method m_method = Method::Linear;

public:
    DisplayMapping();
    ~DisplayMapping();

    void SetupRendergraph( SceneViewFrameData& data, DisplayMappingContext& ctx ) const;

    void DisplayMappingPass( RHICommandList& cmd_list, const SceneViewFrameData& data, const DisplayMappingContext& ctx ) const;

    void DebugUI();
};