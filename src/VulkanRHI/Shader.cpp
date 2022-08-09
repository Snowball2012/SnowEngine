#include "StdAfx.h"

#include "Shader.h"

#include <dxcapi.h>

Shader::Shader(std::wstring filename, RHI::ShaderFrequency frequency, std::string entry_point, std::vector<ShaderDefine> defines, ComPtr<IDxcBlob> bytecode)
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

Shader::Shader(const ShaderSourceFile& source, RHI::ShaderFrequency frequency, std::string entry_point, std::vector<ShaderDefine> defines)
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

void Shader::AddRef()
{
}

void Shader::Release()
{
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
