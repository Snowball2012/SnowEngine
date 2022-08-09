#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

struct ShaderDefine
{
    std::string name;
    std::string value;
};

class Shader : public RHIShader
{
private:
    ComPtr<struct IDxcBlob> m_bytecode_blob = nullptr;

    std::wstring m_filename;
    std::string m_entrypoint;
    RHI::ShaderFrequency m_frequency;
    std::vector<ShaderDefine> m_defines;

public:

    Shader(std::wstring filename, RHI::ShaderFrequency frequency, std::string entry_point, std::vector<ShaderDefine> defines, ComPtr<struct IDxcBlob> bytecode);
    Shader(const class ShaderSourceFile& source, RHI::ShaderFrequency frequency, std::string entry_point, std::vector<ShaderDefine> defines = std::vector<ShaderDefine>());

    virtual void* GetNativeBytecode() const override { return m_bytecode_blob.Get(); }

    // Inherited via RHIShader
    virtual void AddRef() override;
    virtual void Release() override;

    const wchar_t* GetFileName() const { return m_filename.c_str(); }
    const char* GetEntryPoint() const { return m_entrypoint.c_str(); }

private:
    void Compile(const ShaderSourceFile* source);

};

class ShaderSourceFile
{
private:
    std::wstring m_filename;
    ComPtr<struct IDxcBlobEncoding> m_blob = nullptr;

public:
    ShaderSourceFile(std::wstring filename, ComPtr<IDxcBlobEncoding> blob);

    IDxcBlobEncoding* GetNativeBlob() const { return m_blob.Get(); }
    const wchar_t* GetFilename() const { return m_filename.c_str(); }
};

class ShaderCompiler
{
private:
    static std::unique_ptr<ShaderCompiler> m_shared_instance;

    ComPtr<struct IDxcCompiler> m_dxc_compiler = nullptr;
    ComPtr<struct IDxcUtils> m_dxc_utils = nullptr;
    ComPtr<struct IDxcIncludeHandler> m_dxc_include_header = nullptr;

public:
    static ShaderCompiler* Get();

    std::unique_ptr<ShaderSourceFile> LoadSourceFile(std::wstring filename);
    ComPtr<IDxcBlob> CompileFromSource(const ShaderSourceFile& source, RHI::ShaderFrequency frequency, const std::string& entry_point, const span<const ShaderDefine>& defines);

private:
    ShaderCompiler();
};