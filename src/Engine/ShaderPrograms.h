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
    std::variant<RHIGraphicsPipelinePtr, RHIRaytracingPipelinePtr> m_pso;

    ShaderProgramType m_type = ShaderProgramType::Compute;

public:
    virtual ~ShaderProgram() {}

    virtual void BindToCmdList( RHICommandList& cmd_list ) const
    {
        switch ( m_type )
        {
        case ShaderProgramType::Raster:
            cmd_list.SetPSO( *std::get<RHIGraphicsPipelinePtr>( m_pso ) );
            break;
        case ShaderProgramType::Raytracing:
            cmd_list.SetPSO( *std::get<RHIRaytracingPipelinePtr>( m_pso ) );
        default:
            NOTIMPL;
        }
    }

    virtual bool UsesSceneViewDescset() const = 0;

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
        RHIFormat output_format = RHIFormat::Undefined;
        RHIRenderTargetView* output = nullptr;
        RHITextureROView* input = nullptr;
        RHISampler* sampler = nullptr;
        glm::uvec2 extent = glm::uvec2( 0, 0 );
    };
    BlitTextureProgram();

    virtual void BindToCmdList( RHICommandList& cmd_list ) const override { NOTIMPL; }

    bool Run( RHICommandList& cmd_list, Rendergraph& rg, const Params& parms ) const;

    virtual bool UsesSceneViewDescset() const override
    {
        return false;
    }

private:

    const RHIGraphicsPipeline* GetPSO( RHIFormat output_format ) const;
};