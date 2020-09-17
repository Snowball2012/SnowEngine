// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "stdafx.h"

#include "PSOCache.h"

#include <d3d12.h>
#include <dxgi1_6.h>

#include <d3dx12/d3dx12.h>

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


	// create a device
	CComPtr<IDXGIFactory7> dxgi_factory;
	if ( !SE_ENSURE_HRES( CreateDXGIFactory1( IID_PPV_ARGS( &dxgi_factory ) ) ) )
		return 4;

	// Try to create hardware device.
	CComPtr<ID3D12Device> device;
	HRESULT hardware_result;
	hardware_result = D3D12CreateDevice(
		nullptr,             // default adapter
		D3D_FEATURE_LEVEL_12_1,
		IID_PPV_ARGS( &device ) );


	// Fallback to WARP device.
	if ( FAILED( hardware_result ) )
	{
		ComPtr<IDXGIAdapter> warp_adapter;
		if ( !SE_ENSURE_HRES( dxgi_factory->EnumWarpAdapter( IID_PPV_ARGS( &warp_adapter ) ) ) )
			return 5;

		if ( !SE_ENSURE_HRES( D3D12CreateDevice(
				warp_adapter.Get(),
				D3D_FEATURE_LEVEL_12_1,
				IID_PPV_ARGS( &device ) ) ) )
			return 6;
	}

	
	PSOCache cache( ComPtr<ID3D12Device>( device.p ), compiler, include_handler );

	CComPtr<ID3D12RootSignature> rootsig;

	{
		constexpr int nparams = 1;

		CD3DX12_ROOT_PARAMETER slot_root_parameter[nparams];

		CD3DX12_DESCRIPTOR_RANGE desc_table[1];
		desc_table[0].Init( D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0 );
		slot_root_parameter[0].InitAsDescriptorTable( 1, desc_table );

		CD3DX12_ROOT_SIGNATURE_DESC root_sig_desc( nparams, slot_root_parameter,
			0, nullptr );

		ComPtr<ID3DBlob> serialized_root_sig = nullptr;
		ComPtr<ID3DBlob> error_blob = nullptr;
		HRESULT hr = D3D12SerializeRootSignature( &root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1,
			serialized_root_sig.GetAddressOf(), error_blob.GetAddressOf() );

		if ( !SE_ENSURE_HRES( hr ) )
			return 7;
		if ( error_blob )
		{
			std::cerr << (char*)error_blob->GetBufferPointer() << std::endl;
			return 8;
		}

		if ( !SE_ENSURE_HRES( device->CreateRootSignature(
			0,
			serialized_root_sig->GetBufferPointer(),
			serialized_root_sig->GetBufferSize(),
			IID_PPV_ARGS( &rootsig ) ) ) )
			return 0;

		rootsig->SetName( L"HLSL Test Rootsig" );
	}


	std::string shader1 = 
		"[numthreads(8,8,1)]\n"
		"void main( uint3 thread : SV_DispatchThreadID ) { int i = 0; }\n";
	std::string shader2 = 
		"[numthreads(8,8,1)]\n"
		"void main( uint3 thread : SV_DispatchThreadID ) { int i = 0; }\n";

	std::string shader3 = 
		"[numthreads(8,12,1)]\n"
		"void main( uint3 thread : SV_DispatchThreadID ) { int i = 0; }\n";

	std::string shader_failed = 
		"[numthreads(8,12,1)]\n"
		"int void main( uint3 thread : SV_DispatchThreadID ) { int i = 0; }\n";

	PSOCache::CompilationRequest req;
	req.rootsig = rootsig;

	req.shader_text = std::move( shader1 );
	auto pso_res1 = cache.CompilePSO( std::move( req ) );

	req.shader_text = std::move( shader2 );
	auto pso_res2 = cache.CompilePSO( std::move( req ) );

	req.shader_text = std::move( shader3 );
	auto pso_res3 = cache.CompilePSO( std::move( req ) );

	req.shader_text = std::move( shader_failed );
	auto pso_res_failed = cache.CompilePSO( std::move( req ) );

	pso_res1.wait();
	pso_res2.wait();
	pso_res3.wait();
	pso_res_failed.wait();

	if ( !pso_res1.get() || !pso_res2.get() || !pso_res3.get() )
	{
		std::cerr << "can't compile valid psos\n";
		return 20;
	}

	if ( pso_res1.get().Get() != pso_res2.get().Get() )
	{
		std::cerr << "pso hashing does not work\n";
		return 21;
	}


	if ( pso_res3.get().Get() == pso_res2.get().Get() )
	{
		std::cerr << "pso hashing does not work (same pso for different shaders\n";
		return 22;
	}

	if ( pso_res_failed.get() )
	{
		std::cerr << "valid pso for invalid shader\n";
		return 23;
	}


	/*
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


	*/
	std::cout << "everything is ok!" << std::endl;

	return 0;
}

