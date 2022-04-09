﻿#include "stdafx.h"

#include "RayTracedDirectShadows.h"

GenerateRaytracedShadowmaskPass::GenerateRaytracedShadowmaskPass(ID3D12Device& device)
{
    m_global_root_signature = BuildRootSignature(device);
}

ComPtr<ID3D12RootSignature> GenerateRaytracedShadowmaskPass::BuildRootSignature(ID3D12Device& device)
{
    /*
     * Global rootsig for direct shadows
     * 0 - depth buffer
     * 1- tlas
     * 2 - output uav
     * 3 - pass cbv
     * 4 - constants (light dir)
     **/

    constexpr int nparams = 5;
    
    CD3DX12_ROOT_PARAMETER slot_root_parameter[nparams];

    CD3DX12_DESCRIPTOR_RANGE desc_table[2];
    desc_table[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    desc_table[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

    slot_root_parameter[0].InitAsDescriptorTable(1, &desc_table[0]);
    slot_root_parameter[1].InitAsShaderResourceView(1);
    slot_root_parameter[2].InitAsDescriptorTable(1, &desc_table[1]);
    slot_root_parameter[3].InitAsConstantBufferView(0);
    slot_root_parameter[4].InitAsConstants(3,1);

    CD3DX12_ROOT_SIGNATURE_DESC root_sig_desc(nparams, slot_root_parameter);

    ComPtr<ID3DBlob> serialized_root_sig = nullptr;
    ComPtr<ID3DBlob> error_blob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature( &root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                              serialized_root_sig.GetAddressOf(), error_blob.GetAddressOf() );

    if ( error_blob )
    {
        OutputDebugStringA( (char*)error_blob->GetBufferPointer() );
    }
    ThrowIfFailedH( hr );

    ComPtr<ID3D12RootSignature> rootsig;

    ThrowIfFailedH( device.CreateRootSignature(
        0,
        serialized_root_sig->GetBufferPointer(),
        serialized_root_sig->GetBufferSize(),
        IID_PPV_ARGS( &rootsig ) ) );

    rootsig->SetName( L"RaytracedDirectShadowsPass Rootsig" );

    return rootsig;    
}

inline void GenerateRaytracedShadowmaskPass::Draw(const Context& context, IGraphicsCommandList& cmd_list)
{
    cmd_list.SetComputeRootSignature(m_global_root_signature.Get());
    
    cmd_list.SetComputeRootDescriptorTable(0, context.depth_srv);
    cmd_list.SetComputeRootShaderResourceView(1, context.tlas);
    cmd_list.SetComputeRootDescriptorTable(2, context.shadowmask_uav);
    cmd_list.SetComputeRootConstantBufferView(3, context.pass_cb);
    cmd_list.SetComputeRoot32BitConstants(4, 3, &context.light_dir, 0);
    
    D3D12_DISPATCH_RAYS_DESC dispatch_rays_desc;
    dispatch_rays_desc.Depth = 1;
    dispatch_rays_desc.Width = context.uav_width;
    dispatch_rays_desc.Height = context.uav_height;
    
    auto empty_arg = D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE{{}, {}, {}};
    dispatch_rays_desc.CallableShaderTable = empty_arg;
    dispatch_rays_desc.HitGroupTable = empty_arg;
    dispatch_rays_desc.MissShaderTable = empty_arg;
    dispatch_rays_desc.RayGenerationShaderRecord = D3D12_GPU_VIRTUAL_ADDRESS_RANGE{{},{}};
    
    cmd_list.DispatchRays(&dispatch_rays_desc);
    NOTIMPL;
}
