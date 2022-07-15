#include "stdafx.h"

#include "Shader.h"

#include "span.h"

Shader::Shader(std::wstring filename, ShaderFrequency frequency, std::wstring entry_point, std::vector<ShaderDefine> defines, ComPtr<IDxcBlob> bytecode)
    : m_bytecode_blob(std::move(bytecode))
    , m_filename(std::move(filename)), m_entrypoint(std::move(entry_point))
    , m_frequency(frequency)
    , m_defines(std::move(defines))
{
    if (!bytecode)
    {
        Compile(nullptr);
    }
}

Shader::Shader(const ShaderSourceFile& source, ShaderFrequency frequency, std::wstring entry_point, std::vector<ShaderDefine> defines)
    : m_filename(source.GetFilename()), m_entrypoint(std::move(entry_point)), m_frequency(frequency), m_defines(std::move(defines))
{
    Compile(&source);
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

ComPtr<IDxcBlob> ShaderCompiler::CompileFromSource(const ShaderSourceFile& source,
    ShaderFrequency frequency, const std::wstring& entry_point, const span<const ShaderDefine>& defines)
{
    if (!SE_ENSURE(m_dxc_compiler))
        return nullptr;

    LPCWSTR profiles[uint32_t(ShaderFrequency::Count)] =
    {
        L"vs_6_1",
        L"ps_6_1",
        L"cs_6_1",
        L"lib_6_1"
    };

    std::vector<DxcDefine> dxc_defines;
    dxc_defines.reserve(defines.size());
    for (const auto& define : defines)
    {
        auto& dxc_define = dxc_defines.emplace_back();
        dxc_define.Name = define.name.c_str();
        dxc_define.Value = define.value.empty() ? nullptr : define.value.c_str();
    }

    // generate spirv
    std::vector<LPCWSTR> args = { L"-spirv" };

    ComPtr<IDxcOperationResult> result = nullptr;
    if (!SE_ENSURE_HRES(m_dxc_compiler->Compile(
        source.GetNativeBlob(),
        source.GetFilename(), entry_point.c_str(),
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
