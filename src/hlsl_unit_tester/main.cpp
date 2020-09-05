// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "stdafx.h"

#include "PSOCache.h"

/*
class HLSLFile
{
public:
	
	static std::optional<HLSLFile> Create( std::wstring_view path );

private:
	HLSLFile( std::wstring_view path );
};


class HLSLFunc
{
public:
	static std::optional<HLSLFunc> Create( std::string_view name, const HLSLFile& file );

private:
	HLSLFunc( std::string_view name, const HLSLFile& file );
};


class HLSLInput
{
public:
	template<typename T>
	bool Set( std::string param_name, const T& val );
};


class HLSLOutput
{
public:
	template<typename T>
	bool GetRetval( T& v ) const;
};


class IHLSLTest
{
public:
	virtual ~IHLSLTest() {}

	// Each instance of HLSLInput means one fuction invocation.
	// The number of HLSLInputs will match the number of HLSLOutputs in IHLSLTest::Assert, one for each invocation
	// returning false on Arrange or Assert means the test has failed
	virtual bool Arrange( std::vector<HLSLInput>& inputs ) = 0;
	virtual bool Assert( const std::vector<HLSLOutput>& output ) = 0;
};


class ExampleTest : public IHLSLTest
{
public:
	virtual bool Arrange( std::vector<HLSLInput>& inputs ) override
	{
		auto& input = inputs.emplace_back();

		if ( !input.Set( "color", DirectX::XMFLOAT3( 0, 0, 0 ) ) )
			return false; // can't set the color for some reason

		return true;
	}

	virtual bool Assert( const std::vector<HLSLOutput>& output ) override
	{
		if ( !SE_ENSURE( output.size() == 1 ) )
			return false;

		float res;
		if ( !output[0].GetRetval( res ) )
			return false;

		return SE_ENSURE( res == 0 ); // returning "true" at this point will mean the test has passed
	}
};


void test()
{
	// setup

	constexpr std::wstring_view path = L"../../shaders/lib/colorspaces.hlsli";
	auto file = HLSLFile::Create( path );
	if ( ! SE_ENSURE( file.has_value() ) )
	{
		std::wcerr << L"Could not load HLSL file at " << path << std::endl;
		return;
	}

	constexpr std::string_view func_name = "percieved_brightness";
	auto foo = HLSLFunc::Create( func_name, *file );
	if (!SE_ENSURE(foo.has_value()))
	{
		std::wcerr << L"Could not load HLSL function \"" << std::wstring( func_name.cbegin(), func_name.cend() ) << L"\" from file " << path << std::endl;
		return;
	}


}
*/

int main()
{
	CComPtr<IDxcUtils> dxc_utils;
	CComPtr<IDxcCompiler3> compiler;
	CComPtr<IDxcIncludeHandler> include_handler;
	if ( !SE_ENSURE_HRES( DxcCreateInstance( CLSID_DxcUtils, IID_PPV_ARGS( &dxc_utils ) ) ) )
		return 1;
	if ( !SE_ENSURE_HRES( DxcCreateInstance( CLSID_DxcCompiler, IID_PPV_ARGS( &compiler ) ) ) )
		return 2;
	if ( !SE_ENSURE_HRES( dxc_utils->CreateDefaultIncludeHandler(&include_handler) ) )
		return 3;

	CComPtr<IDxcBlobEncoding> src_blob = nullptr;
	if ( !SE_ENSURE_HRES( dxc_utils->LoadFile( L"../../shaders/lib/colorspaces.hlsli", nullptr, &src_blob ) ) )
		return 4;
	DxcBuffer src_buffer;
	src_buffer.Ptr = src_blob->GetBufferPointer();
	src_buffer.Size = src_blob->GetBufferSize();
	src_buffer.Encoding = DXC_CP_ACP; // Assume BOM says UTF8 or UTF16 or this is ANSI text.


	std::cout << (const char*)src_buffer.Ptr << std::endl;

	LPCWSTR args[] =
	{
		L"/T",
		L"lib_6_1",
		L"/Vd",
		L"/I",
		L"../../shaders/lib",
		L"/default-linkage",
		L"external",
	};

	CComPtr<IDxcResult> compilation_result;
	if ( !SE_ENSURE_HRES( compiler->Compile( &src_buffer, args, _countof(args), include_handler, IID_PPV_ARGS( &compilation_result ) ) ) )
		return 1;

	HRESULT compilation_status;
	if ( !SE_ENSURE_HRES( compilation_result->GetStatus( &compilation_status ) ) )
		return 1;

	if ( FAILED( compilation_status ) )
	{
		CComPtr<IDxcBlobEncoding> errors;
		if ( !SE_ENSURE_HRES( compilation_result->GetErrorBuffer( &errors ) ) )
			return 1;
		const char* ascii_msg = reinterpret_cast<const char*>( errors->GetBufferPointer() );
		std::cerr << ascii_msg << std::endl;
		return 1;
	}

	CComPtr<IDxcBlob> reflection_data;
	if ( !SE_ENSURE_HRES( compilation_result->GetOutput( DXC_OUT_REFLECTION, IID_PPV_ARGS( &reflection_data ), nullptr ) ) )
		return 1;

	DxcBuffer reflection_buffer;
	reflection_buffer.Ptr = reflection_data->GetBufferPointer();
	reflection_buffer.Size = reflection_data->GetBufferSize();
	reflection_buffer.Encoding = 0;
	CComPtr<ID3D12LibraryReflection> reflection;
	if ( !SE_ENSURE_HRES( dxc_utils->CreateReflection( &reflection_buffer, IID_PPV_ARGS( &reflection ) ) ) )
		return 1;

	D3D12_LIBRARY_DESC lib_desc;
	if ( !SE_ENSURE_HRES( reflection->GetDesc( &lib_desc ) ) )
		return 1;

	for ( UINT i = 0; i < lib_desc.FunctionCount; ++i )
	{
		ID3D12FunctionReflection* function_refl = reflection->GetFunctionByIndex( i );
		if ( !SE_ENSURE( function_refl ) )
			return 1;

		D3D12_FUNCTION_DESC function_desc;
		if ( !SE_ENSURE_HRES( function_refl->GetDesc( &function_desc ) ) )
			return 1;

		std::cout << function_desc.Name << std::endl;
	}

	std::cout << "everything is ok!" << std::endl;

	PSOCache cache;

	return 0;
}

