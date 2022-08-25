#include "StdAfx.h"

#include "Shader.h"

#include <dxcapi.h>

#include "VulkanRHIImpl.h"

IMPLEMENT_RHI_OBJECT(Shader)

Shader::Shader(VulkanRHI* rhi, std::wstring filename, RHI::ShaderFrequency frequency, std::string entry_point, std::vector<ShaderDefine> defines, ComPtr<IDxcBlob> bytecode)
    : m_bytecode_blob(std::move(bytecode))
    , m_filename(std::move(filename)), m_entrypoint(std::move(entry_point))
    , m_frequency(frequency)
    , m_defines(std::move(defines))
    , m_rhi(rhi)
{
    if (!bytecode)
    {
        Compile(nullptr);
    }
    CreateVkStage();
}

Shader::Shader(VulkanRHI* rhi, const ShaderSourceFile& source, RHI::ShaderFrequency frequency, std::string entry_point, std::vector<ShaderDefine> defines)
    : m_filename(source.GetFilename()), m_entrypoint(std::move(entry_point)), m_frequency(frequency), m_defines(std::move(defines)), m_rhi(rhi)
{
    Compile(&source);
    CreateVkStage();
}

Shader::~Shader()
{
    if (m_vk_stage.module != VK_NULL_HANDLE)
        vkDestroyShaderModule(m_rhi->GetDevice(), m_vk_stage.module, nullptr);
}

std::unique_ptr<ShaderCompiler> ShaderCompiler::m_shared_instance(nullptr);

void Shader::Compile(const ShaderSourceFile* source)
{
    auto* compiler = ShaderCompiler::Get();

    if (!SE_ENSURE(compiler))
        throw std::runtime_error("failed to get shader compiler");

    std::unique_ptr<ShaderSourceFile> source_holder = nullptr;
    if (!source)
    {
        source_holder = compiler->LoadSourceFile(m_filename);
        source = source_holder.get();
    }

    if (SE_ENSURE(source))
        m_bytecode_blob = compiler->CompileFromSource(*source, m_frequency, m_entrypoint, make_span(m_defines));
    else
        throw std::runtime_error("no source file");

    if (!m_bytecode_blob)
        throw std::runtime_error("shader compilation failed");
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
        L"vs_6_1",
        L"ps_6_1",
        L"cs_6_1",
        L"lib_6_1"
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
    std::vector<LPCWSTR> args = { L"-spirv" };

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
        std::cerr << diagnostics;
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
    if (m_vk_desc_layout)
        vkDestroyDescriptorSetLayout(m_rhi->GetDevice(), m_vk_desc_layout, nullptr);
}

VulkanShaderBindingLayout::VulkanShaderBindingLayout(VulkanRHI* rhi, const RHI::ShaderBindingLayoutInfo& info)
    : m_rhi(rhi)
{
    boost::container::small_vector<VkDescriptorSetLayoutBinding, 16> vk_bindings;
    vk_bindings.resize(info.binding_count);

    for (size_t i = 0; i < info.binding_count; ++i)
    {
        auto& vk_bind = vk_bindings[i];
        auto& rhi_bind = info.bindings[i];

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

    VK_VERIFY(vkCreateDescriptorSetLayout(m_rhi->GetDevice(), &layout_info, nullptr, &m_vk_desc_layout));

    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &m_vk_desc_layout;
    pipeline_layout_info.pushConstantRangeCount = 0;

    VK_VERIFY(vkCreatePipelineLayout(m_rhi->GetDevice(), &pipeline_layout_info, nullptr, &m_vk_pipeline_layout));
}
