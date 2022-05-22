#include "stdafx.h"

#include "RayTracedDirectShadows.h"

#include "engine/Shader.h"

GenerateRaytracedShadowmaskPass::GenerateRaytracedShadowmaskPass(ID3D12Device& device)
{
    m_global_root_signature = BuildRootSignature(device, m_global_root_signature_serialized);
    ThrowIfFailedH(device.QueryInterface(IID_PPV_ARGS(m_rt_device.GetAddressOf())));
    BuildRaytracingPSO(*m_rt_device.Get());
    GenerateShaderTable(device);    
}

ComPtr<ID3D12RootSignature> GenerateRaytracedShadowmaskPass::BuildRootSignature(ID3D12Device& device, ComPtr<ID3DBlob>& serialized_rs)
{
    /*
     * Global rootsig for direct shadows
     * 0 - depth buffer
     * 1- tlas
     * 2 - output uav
     * 3 - pass cbv
     * 4 - constants (light dir)
     **/

    constexpr int nparams = 4;
    
    CD3DX12_ROOT_PARAMETER slot_root_parameter[nparams];

    CD3DX12_DESCRIPTOR_RANGE desc_table[2];
    desc_table[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    desc_table[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

    slot_root_parameter[0].InitAsDescriptorTable(1, &desc_table[0]);
    slot_root_parameter[1].InitAsShaderResourceView(1);
    slot_root_parameter[2].InitAsDescriptorTable(1, &desc_table[1]);
    slot_root_parameter[3].InitAsConstantBufferView(0);

    CD3DX12_ROOT_SIGNATURE_DESC root_sig_desc(nparams, slot_root_parameter);

    ComPtr<ID3DBlob> error_blob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature( &root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                              serialized_rs.GetAddressOf(), error_blob.GetAddressOf() );

    if ( error_blob )
    {
        OutputDebugStringA( (char*)error_blob->GetBufferPointer() );
    }
    ThrowIfFailedH( hr );

    ComPtr<ID3D12RootSignature> rootsig;

    ThrowIfFailedH( device.CreateRootSignature(
        0,
        serialized_rs->GetBufferPointer(),
        serialized_rs->GetBufferSize(),
        IID_PPV_ARGS( &rootsig ) ) );

    rootsig->SetName( L"RaytracedDirectShadowsPass Rootsig" );

    return rootsig;    
}

void GenerateRaytracedShadowmaskPass::BuildRaytracingPSO(ID3D12Device5& device)
{
    std::vector<D3D12_STATE_SUBOBJECT> subobjects;
    subobjects.reserve(10);

    ShaderCompiler* compiler = ShaderCompiler::Get();
    if (!SE_ENSURE(compiler))
        return;
    
    std::unique_ptr<ShaderSourceFile> raytracing_source = compiler->LoadSourceFile(L"shaders/raytracing.hlsl");
    if (!SE_ENSURE(raytracing_source))
        return;

    Shader raygen_shader(*raytracing_source, ShaderFrequency::Raytracing, L"DirectShadowmaskRGS");
    Shader anyhit_shader(*raytracing_source, ShaderFrequency::Raytracing, L"DirectShadowmaskAHS");
    Shader miss_shader(*raytracing_source, ShaderFrequency::Raytracing, L"DirectShadowmaskMS");

    ShaderLibrarySubobjectInfo subobject_infos[3]
    {
        raygen_shader.CreateSubobjectInfo(),
        miss_shader.CreateSubobjectInfo(),
        anyhit_shader.CreateSubobjectInfo()
    };

    for (auto& it : subobject_infos)
    {
        it.lib_desc.pExports = &it.export_desc;
        it.subobject.pDesc = &it.lib_desc;
        subobjects.emplace_back(it.subobject);
    }

    D3D12_HIT_GROUP_DESC hit_group_desc = {};
    hit_group_desc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
    hit_group_desc.AnyHitShaderImport = anyhit_shader.GetEntryPoint();
    hit_group_desc.HitGroupExport = L"HitGroup";

    {
        auto& hit_group_subobj = subobjects.emplace_back();
        hit_group_subobj.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
        hit_group_subobj.pDesc = &hit_group_desc;
    }

    D3D12_RAYTRACING_PIPELINE_CONFIG conf = {};
    conf.MaxTraceRecursionDepth = 1;
    {
        auto& pipeline_config_subobj = subobjects.emplace_back();
        pipeline_config_subobj.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
        pipeline_config_subobj.pDesc = &conf;
    }

    D3D12_RAYTRACING_SHADER_CONFIG shader_conf = {};
    shader_conf.MaxAttributeSizeInBytes = 8;
    shader_conf.MaxPayloadSizeInBytes = 8;
    {
        auto& shader_conf_subobj = subobjects.emplace_back();
        shader_conf_subobj.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
        shader_conf_subobj.pDesc = &shader_conf;
    }

    const wchar_t* shader_payload_exports[] =
    {
        L"DirectShadowmaskRGS",
        L"DirectShadowmaskMS",
        L"HitGroup"
    };
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION association_desc = {};
    association_desc.NumExports = _countof(shader_payload_exports);
    association_desc.pExports = shader_payload_exports;
    association_desc.pSubobjectToAssociate = &subobjects.back();
    {
        auto& assoc_so = subobjects.emplace_back();
        assoc_so.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
        assoc_so.pDesc = &association_desc;
    }

    D3D12_GLOBAL_ROOT_SIGNATURE global_rs_desc;
    global_rs_desc.pGlobalRootSignature = m_global_root_signature.Get();
    {
        auto& global_rs = subobjects.emplace_back();
        global_rs.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
        global_rs.pDesc = &global_rs_desc;
    }
    
    
    D3D12_STATE_OBJECT_DESC rtpso_desc = {};
    rtpso_desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
    rtpso_desc.NumSubobjects = UINT(subobjects.size());
    rtpso_desc.pSubobjects = subobjects.data();

    SE_ENSURE_HRES(device.CreateStateObject(&rtpso_desc, IID_PPV_ARGS(m_rtpso.GetAddressOf())));

    SE_ENSURE_HRES(m_rtpso->QueryInterface(IID_PPV_ARGS(m_rtpso_info.GetAddressOf())));
    
}

void GenerateRaytracedShadowmaskPass::Draw(const Context& context, IGraphicsCommandList& cmd_list)
{
    cmd_list.SetPipelineState1(m_rtpso.Get());
    cmd_list.SetComputeRootSignature(m_global_root_signature.Get());
    
    cmd_list.SetComputeRootDescriptorTable(0, context.depth_srv);
    cmd_list.SetComputeRootShaderResourceView(1, context.tlas);
    cmd_list.SetComputeRootDescriptorTable(2, context.shadowmask_uav);
    cmd_list.SetComputeRootConstantBufferView(3, context.pass_cb);
    
    D3D12_DISPATCH_RAYS_DESC dispatch_rays_desc;
    dispatch_rays_desc.Depth = 1;
    dispatch_rays_desc.Width = context.uav_width;
    dispatch_rays_desc.Height = context.uav_height;
    
    auto empty_arg = D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE{{}, {}, {}};
    dispatch_rays_desc.CallableShaderTable = empty_arg;
    dispatch_rays_desc.HitGroupTable = D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE{m_hitgroup_shader_record, m_shader_record_size, m_shader_record_size};
    dispatch_rays_desc.MissShaderTable = D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE{m_miss_shader_record, m_shader_record_size, m_shader_record_size};
    dispatch_rays_desc.RayGenerationShaderRecord = D3D12_GPU_VIRTUAL_ADDRESS_RANGE{m_raygen_shader_record, m_shader_record_size};
    
    cmd_list.DispatchRays(&dispatch_rays_desc);
}

void GenerateRaytracedShadowmaskPass::GenerateShaderTable(ID3D12Device& device)
{
    const auto properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    const auto buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(CalcAlignedSize(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES * 3, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT));
    ThrowIfFailedH(device.CreateCommittedResource(
        &properties, D3D12_HEAP_FLAG_NONE, &buffer_desc, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&m_shader_table)));
    m_shader_table->SetName(L"DirectRaytracedShadowsShadertable");

    uint8_t* mapped_data;
    CD3DX12_RANGE read_range(0, 0);
    ThrowIfFailedH(m_shader_table->Map(0, &read_range, (void**)&mapped_data));
    
    memcpy(mapped_data, m_rtpso_info->GetShaderIdentifier(L"DirectShadowmaskRGS"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    m_raygen_shader_record = m_shader_table->GetGPUVirtualAddress();
    mapped_data += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    
    memcpy(mapped_data, m_rtpso_info->GetShaderIdentifier(L"DirectShadowmaskMS"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    m_miss_shader_record = m_raygen_shader_record + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    mapped_data += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    
    memcpy(mapped_data, m_rtpso_info->GetShaderIdentifier(L"HitGroup"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    m_hitgroup_shader_record = m_miss_shader_record + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    m_shader_record_size = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    
    m_shader_table->Unmap(0, nullptr);    
}
