#include "stdafx.h"

#include "Shader.h"

Shader::Shader(std::wstring filename, std::wstring entry_point, ComPtr<IDxcBlob> bytecode)
    : m_filename(std::move(filename)), m_entrypoint(std::move(entry_point))
    , m_bytecode_blob(std::move(bytecode))
{
    if (!bytecode)
    {
        Compile(nullptr);
    }
}

Shader::Shader(const ShaderSourceFile& source, std::wstring entry_point)
    : m_filename(source.GetFilename()), m_entrypoint(std::move(entry_point))
{
    Compile(&source);
}

std::unique_ptr<ShaderCompiler> ShaderCompiler::m_shared_instance(nullptr);

ShaderLibrarySubobjectInfo Shader::CreateSubobjectInfo() const
{
    ShaderLibrarySubobjectInfo res = {};
    
    auto& export_desc = res.export_desc;
    export_desc.Name = GetEntryPoint();
    export_desc.ExportToRename = GetEntryPoint();
    export_desc.Flags = D3D12_EXPORT_FLAG_NONE;

    auto& lib_desc = res.lib_desc;
    lib_desc.NumExports = 1;
    lib_desc.pExports = &export_desc;
    lib_desc.DXILLibrary.BytecodeLength = m_bytecode_blob->GetBufferSize();
    lib_desc.DXILLibrary.pShaderBytecode = m_bytecode_blob->GetBufferPointer();

    auto& suboobj = res.subobject;
    suboobj.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    suboobj.pDesc = &lib_desc;

    return res;
}

void Shader::Compile(const ShaderSourceFile* source)
{
    auto* compiler = ShaderCompiler::Get();
    if (!SE_ENSURE(compiler))
        return;

    std::unique_ptr<ShaderSourceFile> source_holder = nullptr;
    if (!source)
    {
        source_holder = compiler->LoadSourceFile(m_filename);
        source = source_holder.get();
    }
    
    if (SE_ENSURE(source))
        m_bytecode_blob = compiler->CompileFromSource(*source, m_entrypoint);
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
    if (!SE_ENSURE_HRES(m_dxc_utils->LoadFile(filename.c_str(), &code_page, shader_text.GetAddressOf())))
        return nullptr;

    return std::make_unique<ShaderSourceFile>(std::move(filename), std::move(shader_text));
}

ComPtr<IDxcBlob> ShaderCompiler::CompileFromSource(const ShaderSourceFile& source,
    std::wstring entry_point)
{
    if (!SE_ENSURE(m_dxc_compiler))
        return nullptr;

    ComPtr<IDxcOperationResult> result = nullptr;
    if (!SE_ENSURE_HRES(m_dxc_compiler->Compile(
        source.GetNativeBlob(),
        source.GetFilename(), entry_point.c_str(),
        L"lib6_3", nullptr, 0, nullptr, 0,
        m_dxc_include_header.Get(),
        result.GetAddressOf())))
            return nullptr;

    ComPtr<IDxcBlob> bytecode = nullptr;
    if (!SE_ENSURE_HRES(result->GetResult(bytecode.GetAddressOf())))
        return nullptr;

    return std::move(bytecode);    
}

ShaderCompiler::ShaderCompiler()
{
    ComPtr<IDxcUtils> dxc_utils;
    SE_ENSURE_HRES(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxc_utils )));
    SE_ENSURE_HRES(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_dxc_compiler)));
    SE_ENSURE_HRES(dxc_utils->CreateDefaultIncludeHandler(&m_dxc_include_header) );
}
