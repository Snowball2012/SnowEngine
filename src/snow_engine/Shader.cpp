#include "stdafx.h"

#include "Shader.h"

#include "utils/Log.h"
#include <fstream>

Shader::Shader(std::wstring filename, ShaderFrequency frequency, std::wstring entry_point, std::vector<ShaderDefine> defines, ComPtr<IDxcBlob> bytecode)
    : m_filename(std::move(filename)), m_entrypoint(std::move(entry_point))
    , m_bytecode_blob(std::move(bytecode))
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
        m_bytecode_blob = compiler->CompileFromSource(*source, m_frequency, m_entrypoint, make_span(m_defines));
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
    ShaderFrequency frequency, const std::wstring& entry_point, const span<const ShaderDefine>& defines)
{
    if (!SE_ENSURE(m_dxc_compiler))
        return nullptr;

    LPCWSTR profiles[ShaderFrequency::Count] =
    {
        L"vs_6_3",
        L"ps_6_3",
        L"cs_6_3",
        L"lib_6_3"
    };

    std::vector<DxcDefine> dxc_defines;
    dxc_defines.reserve(defines.size());
    for (const auto& define : defines)
    {
        auto& dxc_define = dxc_defines.emplace_back();
        dxc_define.Name = define.name.c_str();
        dxc_define.Value = define.value.empty() ? nullptr : define.value.c_str();
    }

    std::vector<LPCWSTR> args;
    args.push_back( DXC_ARG_DEBUG ); //-Zi
    args.push_back( DXC_ARG_OPTIMIZATION_LEVEL3 );
    args.push_back( DXC_ARG_DEBUG_NAME_FOR_SOURCE );

    ComPtr<IDxcCompilerArgs> args_iface;
    m_dxc_utils->BuildArguments(
        source.GetFilename(),
        entry_point.c_str(),
        profiles[uint8_t( frequency )],
        args.data(),
        UINT( args.size() ),
        dxc_defines.data(),
        UINT( dxc_defines.size() ),
        args_iface.GetAddressOf() );

    ComPtr<IDxcResult> result = nullptr;
    DxcBuffer source_buf = {};
    source_buf.Ptr = source.GetNativeBlob()->GetBufferPointer();
    source_buf.Size = source.GetNativeBlob()->GetBufferSize();
    source_buf.Encoding = 0;

    if (!SE_ENSURE_HRES(m_dxc_compiler->Compile(
        &source_buf,
        args_iface->GetArguments(),
        args_iface->GetCount(),
        m_dxc_include_header.Get(),
        IID_PPV_ARGS( result.GetAddressOf()) )))
            return nullptr;


    ComPtr<IDxcBlob> bytecode = nullptr;
    if (!SE_ENSURE_HRES(result->GetResult(bytecode.GetAddressOf())))
        return nullptr;

    if (!bytecode || bytecode->GetBufferSize() == 0)
    {
        ComPtr<IDxcBlobEncoding> error_buffer;
        result->GetErrorBuffer(error_buffer.GetAddressOf());
        if (error_buffer)
        {
            char* Chars = new char[error_buffer->GetBufferSize() + 1];
            memcpy(Chars, error_buffer->GetBufferPointer(), error_buffer->GetBufferSize());
            Chars[error_buffer->GetBufferSize()] = 0;
            SE_LOG("%s", Chars);
            delete[] Chars;
        }
    }
    else
    {
        ComPtr<IDxcBlob> pdb = nullptr;
        ComPtr<IDxcBlobUtf16> pdb_name = nullptr;
        if ( SE_ENSURE_HRES( result->GetOutput( DXC_OUT_PDB, IID_PPV_ARGS( pdb.GetAddressOf() ), pdb_name.GetAddressOf() ) ) )
        {
            std::ofstream out_file( pdb_name->GetStringPointer(), std::ios_base::binary );
            out_file.write( (const char *)pdb->GetBufferPointer(), pdb->GetBufferSize() );
            out_file.close();
        }
    }

    return std::move(bytecode);    
}

ShaderCompiler::ShaderCompiler()
{
    SE_ENSURE_HRES(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(m_dxc_utils.GetAddressOf())));
    SE_ENSURE_HRES(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(m_dxc_compiler.GetAddressOf())));
    SE_ENSURE_HRES(m_dxc_utils->CreateDefaultIncludeHandler(&m_dxc_include_header) );
}
