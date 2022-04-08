#include <engine/stdafx.h>

#include "RayTracedDirectShadows.h"

inline GenerateRaytracedShadowmaskPass::GenerateRaytracedShadowmaskPass()
{
}

inline void GenerateRaytracedShadowmaskPass::Draw(const Context& context, IGraphicsCommandList& cmd_list)
{
    D3D12_DISPATCH_RAYS_DESC dispatch_rays_desc;
    dispatch_rays_desc.Depth = 1;
    dispatch_rays_desc.Width = 512;
    dispatch_rays_desc.Height = 512;
    auto empty_arg = D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE{{}, {}, {}};
    dispatch_rays_desc.CallableShaderTable = empty_arg;
    dispatch_rays_desc.HitGroupTable = empty_arg;
    dispatch_rays_desc.MissShaderTable = empty_arg;
    dispatch_rays_desc.RayGenerationShaderRecord = D3D12_GPU_VIRTUAL_ADDRESS_RANGE{{},{}};
    
    cmd_list.DispatchRays(&dispatch_rays_desc);
    NOTIMPL;
}