#pragma once

#include "StdAfx.h"

class Rendergraph;

// @todo - make this an asset

enum class ShaderProgramType
{
    Raster,
    Raytracing,
    Compute,
    Mesh
};

class ShaderProgram
{
protected:
    RHIShaderBindingLayoutPtr m_layout = nullptr;

    ShaderProgramType m_type = ShaderProgramType::Compute;

public:
    virtual ~ShaderProgram() {}

protected:
    ShaderProgram() {}
};


class BlitTextureProgram : public ShaderProgram
{
private:
    RHIDescriptorSetLayoutPtr m_dsl = nullptr;

    RHIShaderPtr m_vs = nullptr;
    RHIShaderPtr m_ps = nullptr;

    mutable std::unordered_map<RHIFormat, RHIGraphicsPipelinePtr> m_psos;

public:
    struct Params
    {
        RHIRenderTargetView* output = nullptr;
        RHITextureROView* input = nullptr;
        RHISampler* sampler = nullptr;
    };
    BlitTextureProgram();

    bool Run( RHICommandList& cmd_list, Rendergraph& rg, const Params& parms ) const;

private:

    const RHIGraphicsPipeline* GetPSO( RHIFormat output_format ) const;
};