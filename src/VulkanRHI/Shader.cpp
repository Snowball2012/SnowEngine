#include "StdAfx.h"

#include "Shader.h"

#include <dxcapi.h>

#include "VulkanRHIImpl.h"

#include "ResourceViews.h"

#include "AccelerationStructures.h"

IMPLEMENT_RHI_OBJECT(Shader)

Shader::Shader(VulkanRHI* rhi, std::wstring filename, RHI::ShaderFrequency frequency, std::string entry_point, std::vector<ShaderDefine> defines, ComPtr<IDxcBlob> bytecode)
    : m_bytecode_blob(std::move(bytecode))
    , m_filename(std::move(filename)), m_entrypoint(std::move(entry_point))
    , m_frequency(frequency)
    , m_defines(std::move(defines))
    , m_rhi(rhi)
{
    if ( !m_bytecode_blob )
    {
        VERIFY( Compile( nullptr ) );
    }

    CreateVkStage();

    m_rhi->RegisterLoadedShader( *this );
}

Shader::Shader(VulkanRHI* rhi, const ShaderSourceFile& source, RHI::ShaderFrequency frequency, std::string entry_point, std::vector<ShaderDefine> defines)
    : m_filename(source.GetFilename()), m_entrypoint(std::move(entry_point)), m_frequency(frequency), m_defines(std::move(defines)), m_rhi(rhi)
{
    Compile(&source);
    CreateVkStage();
}

Shader::~Shader()
{
    m_rhi->UnregisterLoadedShader( *this );
    CleanupShaderStage();
}

std::unique_ptr<ShaderCompiler> ShaderCompiler::m_shared_instance(nullptr);

bool Shader::Recompile()
{
    if ( !Compile( nullptr ) )
    {
        return false;
    }

    CleanupShaderStage();

    CreateVkStage();

    return true;
}

bool Shader::Compile(const ShaderSourceFile* source)
{
    auto* compiler = ShaderCompiler::Get();

    if ( !SE_ENSURE( compiler ) )
        return false;

    std::unique_ptr<ShaderSourceFile> source_holder = nullptr;
    if (!source)
    {
        source_holder = compiler->LoadSourceFile(m_filename);
        source = source_holder.get();
    }

    if ( SE_ENSURE( source ) )
        m_bytecode_blob = compiler->CompileFromSource( *source, m_frequency, m_entrypoint, make_span( m_defines ) );
    else
        return false;

    if ( !m_bytecode_blob )
        return false;

    return true;
}

namespace
{
    VkShaderStageFlagBits FrequencyToVkStage(RHI::ShaderFrequency frequency)
    {
        VkShaderStageFlagBits retval = {};

        switch (frequency)
        {
        case RHI::ShaderFrequency::Vertex:
            retval = VK_SHADER_STAGE_VERTEX_BIT;
            break;
        case RHI::ShaderFrequency::Pixel:
            retval = VK_SHADER_STAGE_FRAGMENT_BIT;
            break;
        case RHI::ShaderFrequency::Raygen:
            retval = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
            break;
        case RHI::ShaderFrequency::Miss:
            retval = VK_SHADER_STAGE_MISS_BIT_KHR;
            break;
        case RHI::ShaderFrequency::ClosestHit:
            retval = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
            break;
        default:
            NOTIMPL;
            break;
        }

        return retval;
    }
}

void Shader::CreateVkStage()
{
    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = m_bytecode_blob->GetBufferSize();
    create_info.pCode = reinterpret_cast<const uint32_t*>(m_bytecode_blob->GetBufferPointer());

    VK_VERIFY(vkCreateShaderModule(m_rhi->GetDevice(), &create_info, nullptr, &m_vk_stage.module));

    m_vk_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    m_vk_stage.stage = FrequencyToVkStage(m_frequency);
    m_vk_stage.pName = GetEntryPoint();
}

void Shader::CleanupShaderStage()
{
    if ( m_vk_stage.module != VK_NULL_HANDLE )
        vkDestroyShaderModule( m_rhi->GetDevice(), m_vk_stage.module, nullptr );

    m_vk_stage = VkPipelineShaderStageCreateInfo {};

}

ShaderSourceFile::ShaderSourceFile(std::wstring filename, ComPtr<IDxcBlobEncoding> blob)
    : m_filename(std::move(filename)), m_blob(std::move(blob))
{
}

ShaderCompiler* ShaderCompiler::Get()
{
    if (!m_shared_instance)
        m_shared_instance = std::unique_ptr<ShaderCompiler>(new ShaderCompiler());

    return m_shared_instance.get();
}

std::unique_ptr<ShaderSourceFile> ShaderCompiler::LoadSourceFile(std::wstring filename)
{
    if (!SE_ENSURE(m_dxc_utils))
        return nullptr;

    UINT32 code_page = 0;
    ComPtr<IDxcBlobEncoding> shader_text = nullptr;
    HRESULT res = m_dxc_utils->LoadFile(filename.c_str(), &code_page, shader_text.GetAddressOf());
    if (!SE_ENSURE_HRES(res))
        return nullptr;

    return std::make_unique<ShaderSourceFile>(std::move(filename), std::move(shader_text));
}

namespace
{
    std::wstring WstringFromChar(const char* str)
    {
        // inefficient
        std::wstring retval(size_t(strlen(str)), '\0');
        wchar_t* dst = retval.data();
        while (char c = *str++)
        {
            *dst++ = wchar_t(c);
        }

        return retval;
    }
}

ComPtr<IDxcBlob> ShaderCompiler::CompileFromSource(const ShaderSourceFile& source,
    RHI::ShaderFrequency frequency, const std::string& entry_point, const span<const ShaderDefine>& defines)
{
    if (!SE_ENSURE(m_dxc_compiler))
        return nullptr;

    LPCWSTR profiles[uint32_t(RHI::ShaderFrequency::Count)] =
    {
        L"vs_6_3",
        L"ps_6_3",
        L"cs_6_3",
        L"lib_6_3",
        L"lib_6_3",
        L"lib_6_3"
    };

    std::vector<DxcDefine> dxc_defines;
    dxc_defines.reserve(defines.size());

    std::vector<std::wstring> temp_wstrings;
    for (const auto& define : defines)
    {
        auto& dxc_define = dxc_defines.emplace_back();
        temp_wstrings.emplace_back(WstringFromChar(define.name.c_str()));
        dxc_define.Name = temp_wstrings.back().c_str();
        temp_wstrings.emplace_back(WstringFromChar(define.value.c_str()));
        dxc_define.Value = temp_wstrings.back().empty() ? nullptr : temp_wstrings.back().c_str();
    }

    // generate spirv
    std::vector<LPCWSTR> args = { L"-spirv", L"-fspv-target-env=vulkan1.2", L"-fvk-use-dx-layout", L"-enable-16bit-types"};

    std::wstring entry_point_wstr = WstringFromChar(entry_point.c_str());

    ComPtr<IDxcOperationResult> result = nullptr;
    if (!SE_ENSURE_HRES(m_dxc_compiler->Compile(
        source.GetNativeBlob(),
        source.GetFilename(), entry_point_wstr.c_str(),
        profiles[uint8_t(frequency)], args.data(), uint32_t(args.size()), dxc_defines.data(), uint32_t(dxc_defines.size()),
        m_dxc_include_header.Get(),
        result.GetAddressOf())))
        return nullptr;

    HRESULT status;
    if (!SE_ENSURE_HRES(result->GetStatus(&status)))
        return nullptr;

    ComPtr<IDxcBlobEncoding> error_buffer;
    if (!SE_ENSURE_HRES(result->GetErrorBuffer(error_buffer.GetAddressOf())))
        return nullptr;

    const std::string diagnostics((char*)error_buffer->GetBufferPointer(), error_buffer->GetBufferSize());

    if (status != S_OK)
    {
        SE_LOG_ERROR( VulkanRHI, "Shader compilation failed. Diagnostics:\n%s", diagnostics.c_str() );
        return nullptr;
    }

    ComPtr<IDxcBlob> bytecode = nullptr;
    if (!SE_ENSURE_HRES(result->GetResult(bytecode.GetAddressOf())))
        return nullptr;

    return bytecode;
}

ShaderCompiler::ShaderCompiler()
{
    SE_ENSURE_HRES(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(m_dxc_utils.GetAddressOf())));
    SE_ENSURE_HRES(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(m_dxc_compiler.GetAddressOf())));
    SE_ENSURE_HRES(m_dxc_utils->CreateDefaultIncludeHandler(&m_dxc_include_header));
}


IMPLEMENT_RHI_OBJECT(VulkanShaderBindingLayout)

VulkanShaderBindingLayout::~VulkanShaderBindingLayout()
{
    if (m_vk_pipeline_layout)
        vkDestroyPipelineLayout(m_rhi->GetDevice(), m_vk_pipeline_layout, nullptr);
}

VulkanShaderBindingLayout::VulkanShaderBindingLayout(VulkanRHI* rhi, const RHI::ShaderBindingLayoutInfo& info)
    : m_rhi(rhi)
{
    m_layout_infos.resize(info.table_count);
    boost::container::small_vector<VkDescriptorSetLayout, 16> vk_desc_layouts;
    vk_desc_layouts.resize(info.table_count);

    for (size_t binding_idx = 0; binding_idx < info.table_count; ++binding_idx)
    {
        auto& binding_info = info.tables[binding_idx];
        m_layout_infos[binding_idx] = &RHIImpl(*binding_info);
        vk_desc_layouts[binding_idx] = m_layout_infos[binding_idx]->GetVkDescriptorSetLayout();
    }

    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = uint32_t(vk_desc_layouts.size());
    pipeline_layout_info.pSetLayouts = vk_desc_layouts.data();

    VkPushConstantRange vk_push_constants = {};
    if ( info.push_constants_size > 0 )
    {
        vk_push_constants.offset = 0;
        vk_push_constants.size = uint32_t( CalcAlignedSize( info.push_constants_size, 4 ) );
        vk_push_constants.stageFlags = VK_SHADER_STAGE_ALL;

        pipeline_layout_info.pushConstantRangeCount = 1;
        pipeline_layout_info.pPushConstantRanges = &vk_push_constants;

        m_num_push_constants = vk_push_constants.size / 4;
    }

    VK_VERIFY(vkCreatePipelineLayout(m_rhi->GetDevice(), &pipeline_layout_info, nullptr, &m_vk_pipeline_layout));
}


IMPLEMENT_RHI_OBJECT(VulkanDescriptorSetLayout)

VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout()
{
    if (m_vk_desc_set_layout)
        vkDestroyDescriptorSetLayout(m_rhi->GetDevice(), m_vk_desc_set_layout, nullptr);
}

VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(VulkanRHI* rhi, const RHI::DescriptorSetLayoutInfo& info)
    : m_rhi(rhi)
{
    boost::container::small_vector<VkDescriptorSetLayoutBinding, 16> vk_bindings;
    vk_bindings.resize(info.range_count);

    for (size_t i = 0; i < info.range_count; ++i)
    {
        auto& vk_bind = vk_bindings[i];
        auto& rhi_bind = info.ranges[i];

        if (rhi_bind.count <= 0)
        {
            NOTIMPL; // bindless is not supported right now
        }

        vk_bind.binding = uint32_t(i);
        vk_bind.descriptorCount = uint32_t(rhi_bind.count);
        vk_bind.pImmutableSamplers = nullptr;
        vk_bind.stageFlags = VulkanRHI::GetVkShaderStageFlags(rhi_bind.stages);
        vk_bind.descriptorType = VulkanRHI::GetVkDescriptorType(rhi_bind.type);
    }

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = uint32_t(vk_bindings.size());
    layout_info.pBindings = vk_bindings.data();

    bc::small_vector<VkDescriptorBindingFlags, 4> flags;
    for ( uint32_t i = 0; i < layout_info.bindingCount; ++i )
    {
        flags.emplace_back( VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT );
    }
    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_info = {};
    binding_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    binding_info.pBindingFlags = flags.data();
    binding_info.bindingCount = layout_info.bindingCount;

    layout_info.pNext = &binding_info;
    VK_VERIFY(vkCreateDescriptorSetLayout(m_rhi->GetDevice(), &layout_info, nullptr, &m_vk_desc_set_layout));
}


IMPLEMENT_RHI_OBJECT(VulkanDescriptorSet)

VulkanDescriptorSet::~VulkanDescriptorSet()
{
    if (m_vk_desc_set)
        vkFreeDescriptorSets(m_rhi->GetDevice(), m_rhi->GetVkDescriptorPool(), 1, &m_vk_desc_set);
}

VulkanDescriptorSet::VulkanDescriptorSet(VulkanRHI* rhi, RHIDescriptorSetLayout& layout)
    : m_rhi(rhi)
{
    m_dsl = &RHIImpl(layout);

    VkDescriptorSetLayout vk_layout = m_dsl->GetVkDescriptorSetLayout();

    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = m_rhi->GetVkDescriptorPool();
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &vk_layout;

    VK_VERIFY(vkAllocateDescriptorSets(m_rhi->GetDevice(), &alloc_info, &m_vk_desc_set));
}

void VulkanDescriptorSet::BindUniformBufferView(size_t range_idx, size_t idx_in_range, RHIUniformBufferView& cbv)
{
    auto& vk_cbv = RHIImpl(cbv);

    auto& write_struct = m_pending_writes.emplace_back();
    InitWriteStruct(write_struct, range_idx, idx_in_range);

    write_struct.descriptorCount = 1;
    write_struct.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write_struct.pBufferInfo = vk_cbv.GetVkBufferInfo();

    m_referenced_objects.emplace_back(&vk_cbv);
}

void VulkanDescriptorSet::BindUniformBufferView( size_t range_idx, size_t idx_in_range, const RHIBufferViewInfo& view )
{
    VERIFY_NOT_EQUAL( view.buffer, nullptr );

    auto& write_struct = m_pending_writes.emplace_back();
    InitWriteStruct( write_struct, range_idx, idx_in_range );

    write_struct.descriptorCount = 1;
    write_struct.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    auto& buffer_info = m_buffer_infos.emplace_back();
    auto& vk_cb = RHIImpl( *view.buffer );
    buffer_info.buffer = vk_cb.GetVkBuffer();
    buffer_info.offset = view.offset;
    buffer_info.range = view.range == RHIBufferViewInfo::WHOLE_SIZE ? VK_WHOLE_SIZE : view.range;

    write_struct.pBufferInfo = &buffer_info;

    m_referenced_objects.emplace_back( &vk_cb );
}

void VulkanDescriptorSet::BindTextureROView(size_t range_idx, size_t idx_in_range, RHITextureROView& srv)
{
    auto& vk_srv = RHIImpl(srv);

    auto& write_struct = m_pending_writes.emplace_back();
    InitWriteStruct(write_struct, range_idx, idx_in_range);

    write_struct.descriptorCount = 1;
    write_struct.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write_struct.pImageInfo = vk_srv.GetVkImageInfo();

    m_referenced_objects.emplace_back(&vk_srv);
}

void VulkanDescriptorSet::BindTextureRWView( size_t range_idx, size_t idx_in_range, RHITextureRWView& view )
{
    auto& vk_view = RHIImpl( view );

    auto& write_struct = m_pending_writes.emplace_back();
    InitWriteStruct( write_struct, range_idx, idx_in_range );

    write_struct.descriptorCount = 1;
    write_struct.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write_struct.pImageInfo = vk_view.GetVkImageInfo();

    m_referenced_objects.emplace_back( &vk_view );
}

void VulkanDescriptorSet::BindAccelerationStructure( size_t range_idx, size_t idx_in_range, RHIAccelerationStructure& as )
{
    auto& vk_as = RHIImpl( as );

    auto& write_struct = m_pending_writes.emplace_back();
    InitWriteStruct( write_struct, range_idx, idx_in_range );

    write_struct.descriptorCount = 1;
    write_struct.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    auto& vk_as_info = m_as_infos.emplace_back();
    vk_as_info = {};
    vk_as_info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    vk_as_info.accelerationStructureCount = 1;
    vk_as_info.pAccelerationStructures = vk_as.GetVkASPtr();
    write_struct.pNext = &vk_as_info;

    m_referenced_objects.emplace_back( &vk_as );
}

void VulkanDescriptorSet::BindSampler(size_t range_idx, size_t idx_in_range, RHISampler& sampler)
{
    auto& vk_sampler = RHIImpl(sampler);

    auto& write_struct = m_pending_writes.emplace_back();
    InitWriteStruct(write_struct, range_idx, idx_in_range);

    write_struct.descriptorCount = 1;
    write_struct.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    write_struct.pImageInfo = vk_sampler.GetVkImageInfo();

    m_referenced_objects.emplace_back(&vk_sampler);
}

void VulkanDescriptorSet::BindStructuredBuffer( size_t range_idx, size_t idx_in_range, const RHIBufferViewInfo& view )
{
    VERIFY_NOT_EQUAL( view.buffer, nullptr );

    auto& write_struct = m_pending_writes.emplace_back();
    InitWriteStruct( write_struct, range_idx, idx_in_range );

    write_struct.descriptorCount = 1;
    write_struct.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    auto& buffer_info = m_buffer_infos.emplace_back();
    auto& vk_cb = RHIImpl( *view.buffer );
    buffer_info.buffer = vk_cb.GetVkBuffer();
    buffer_info.offset = view.offset;
    buffer_info.range = view.range == RHIBufferViewInfo::WHOLE_SIZE ? VK_WHOLE_SIZE : view.range;

    write_struct.pBufferInfo = &buffer_info;

    m_referenced_objects.emplace_back( &vk_cb );
}

void VulkanDescriptorSet::FlushBinds()
{
    if (m_pending_writes.empty())
        return;

    vkUpdateDescriptorSets(
        m_rhi->GetDevice(),
        uint32_t(m_pending_writes.size()), m_pending_writes.data(),
        0, nullptr);

    m_pending_writes.clear();
    m_referenced_objects.clear();
    m_as_infos.clear();
    m_buffer_infos.clear();
}

void VulkanDescriptorSet::InitWriteStruct(VkWriteDescriptorSet& write_struct, size_t range_idx, size_t idx_in_range) const
{
    write_struct.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

    write_struct.dstSet = m_vk_desc_set;
    write_struct.dstBinding = uint32_t(range_idx);
    write_struct.dstArrayElement = uint32_t(idx_in_range);
}
