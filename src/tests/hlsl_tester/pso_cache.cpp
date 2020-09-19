// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <boost/test/unit_test.hpp>

#include <hlsl_unit_tester/PSOCache.h>
#include <utils/Assertions.h>

#include <atlbase.h>

#include <dxc/dxcapi.h>
#include <dxc/d3d12shader.h>

#include <d3d12.h>
#include <dxgi1_6.h>

#include <d3dx12/d3dx12.h>

struct Fixture
{
	CComPtr<IDxcCompiler3> compiler;
	CComPtr<IDxcIncludeHandler> include_handler;
	CComPtr<ID3D12Device> device;
	CComPtr<ID3D12RootSignature> rootsig;

	Fixture()
	{
		CComPtr<IDxcUtils> dxc_utils;
		BOOST_REQUIRE( SE_ENSURE_HRES( DxcCreateInstance( CLSID_DxcUtils, IID_PPV_ARGS( &dxc_utils ) ) ) );
		BOOST_REQUIRE( SE_ENSURE_HRES( DxcCreateInstance( CLSID_DxcCompiler, IID_PPV_ARGS( &compiler ) ) ) );
		BOOST_REQUIRE( SE_ENSURE_HRES( dxc_utils->CreateDefaultIncludeHandler(&include_handler) ) );

		// create a device
		CComPtr<IDXGIFactory7> dxgi_factory;
		BOOST_REQUIRE( SE_ENSURE_HRES( CreateDXGIFactory1( IID_PPV_ARGS( &dxgi_factory ) ) ) );

		// Try to create hardware device.
		HRESULT hardware_result;
		hardware_result = D3D12CreateDevice(
			nullptr,             // default adapter
			D3D_FEATURE_LEVEL_12_1,
			IID_PPV_ARGS( &device ) );


		// Fallback to WARP device.
		if ( FAILED( hardware_result ) )
		{
			ComPtr<IDXGIAdapter> warp_adapter;
			BOOST_REQUIRE( SE_ENSURE_HRES( dxgi_factory->EnumWarpAdapter( IID_PPV_ARGS( &warp_adapter ) ) ) );

			BOOST_REQUIRE( SE_ENSURE_HRES( D3D12CreateDevice(
				warp_adapter.Get(),
				D3D_FEATURE_LEVEL_12_1,
				IID_PPV_ARGS( &device ) ) ) );
		}

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

			BOOST_REQUIRE( SE_ENSURE_HRES( hr ) );
			if( error_blob )
				BOOST_CHECK_MESSAGE( 0, (char*)error_blob->GetBufferPointer() );

			BOOST_REQUIRE( SE_ENSURE_HRES( device->CreateRootSignature(
				0,
				serialized_root_sig->GetBufferPointer(),
				serialized_root_sig->GetBufferSize(),
				IID_PPV_ARGS( &rootsig ) ) ) );

			rootsig->SetName( L"HLSL Test Rootsig" );
		}
	}
};


BOOST_AUTO_TEST_SUITE( hlsl_tester )

BOOST_AUTO_TEST_SUITE( pso_cache )

BOOST_FIXTURE_TEST_CASE( basic_tests, Fixture )
{
	PSOCache cache( ComPtr<ID3D12Device>( device.p ), compiler, include_handler );	


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

	BOOST_CHECK( bool( pso_res1.get() && pso_res2.get() && pso_res3.get() ) );
	

	BOOST_CHECK( pso_res1.get().Get() == pso_res2.get().Get() );


	BOOST_CHECK( pso_res3.get().Get() != pso_res2.get().Get() );

	BOOST_CHECK( !pso_res_failed.get() );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()