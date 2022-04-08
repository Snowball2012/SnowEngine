#pragma once

#include <engine/FramegraphResource.h>

class GPUDevice;

class GenerateRaytracedShadowmaskPass
{
public:

    struct Context
    {
    };

    GenerateRaytracedShadowmaskPass();

    void Draw(const Context& context, IGraphicsCommandList& cmd_list);    
};

template<class Framegraph>
class GenerateRaytracedShadowmaskNode : public BaseRenderNode<Framegraph>
{
public:
    using OpenRes = std::tuple<>;
    using WriteRes = std::tuple<>;
    using ReadRes = std::tuple<>;
    using CloseRes = std::tuple<>;
    
private:
    
public:

    virtual void Run(Framegraph& framegraph, IGraphicsCommandList& cmd_list) override;
};

template <class Framegraph>
void GenerateRaytracedShadowmaskNode<Framegraph>::Run(Framegraph& framegraph, IGraphicsCommandList& cmd_list)
{
    OPTICK_EVENT();
    
    PIXBeginEvent(&cmd_list, PIX_COLOR( 200, 210, 230 ), "Raytraced Direct Shadows");

    GenerateRaytracedShadowmaskPass::Context ctx;
    GenerateRaytracedShadowmaskPass pass;

    pass.Draw(ctx, cmd_list);
    
    PIXEndEvent(&cmd_list);
}
