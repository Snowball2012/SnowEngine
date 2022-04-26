#pragma once

class Shader
{
private:
    ComPtr<struct IDxcBlob> m_bytecode_blob = nullptr;

    std::wstring m_filename;
    std::wstring m_entrypoint;
    
public:
    Shader(std::wstring filename, std::wstring entry_point, ComPtr<IDxcBlob> bytecode);
    IDxcBlob* GetNativeBytecode() const { return m_bytecode_blob.Get(); }

    const wchar_t* GetFileName() const { return m_filename.c_str(); }
    const wchar_t* GetEntryPoint() const { return m_entrypoint.c_str(); }

private:
    void Compile();
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
    ComPtr<IDxcBlob> CompileFromSource(const ShaderSourceFile& source, std::wstring entry_point);

private:
    ShaderCompiler();
};