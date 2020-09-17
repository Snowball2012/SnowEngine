// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "stdafx.h"

#include "PSOCache.h"


PSOCache::PSOCache(
	ComPtr<ID3D12Device> device, CComPtr<IDxcCompiler3> compiler,
	CComPtr<IDxcIncludeHandler> include_handler )
	: m_device( std::move( device ) ), m_compiler( std::move( compiler ) )
	, m_include_handler( std::move( include_handler ) )
{
}


PSOCache::CompilationResult PSOCache::CompilePSO( CompilationRequest request )
{
	CompilationResult res;

	if ( ! CheckInnerState() )
		return res;

	const PsoHash hash = CalcHash( request );
	if ( auto it = m_pso_lut.find( hash ); it != m_pso_lut.end() )
		return it->second;

	auto async_res = std::async([&, request]() -> ComPtr<ID3D12PipelineState>
	{
		ComPtr<ID3D12PipelineState> pso;

		D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc{};
		pso_desc.pRootSignature = request.rootsig;

		// ToDo: shader compilation

		DxcBuffer src_buffer;
		src_buffer.Ptr = request.shader_text.data();
		src_buffer.Size = request.shader_text.size();
		src_buffer.Encoding = DXC_CP_ACP; // ansi

		LPCWSTR args[] =
		{
			L"/E",
			L"main",
			L"/T",
			L"cs_6_1",
			L"/Wx",
			L"/O3"
		};

		CComPtr<IDxcResult> compilation_result;
		HRESULT hr;
		if ( !SE_ENSURE_HRES( hr = m_compiler->Compile( &src_buffer, args, UINT32( std::size( args ) ), m_include_handler, IID_PPV_ARGS( &compilation_result ) ) ) )
		{
			std::cerr << "IDxcCompiler3::Compile error #" << hr << std::endl;
			return nullptr;
		}


		ComPtr<IDxcBlobUtf8> error_buffer;
		if ( !SE_ENSURE_HRES( hr = compilation_result->GetOutput( DXC_OUT_ERRORS, IID_PPV_ARGS( error_buffer.GetAddressOf() ), nullptr ) ) )
		{
			std::cerr << "IDxcResult::GetOutput error #" << hr << std::endl;
			return nullptr;
		}

		if ( error_buffer && error_buffer->GetStringLength() > 0 )
		{
			std::cerr << "Shader compilation error : " << error_buffer->GetStringPointer() << std::endl;
			return nullptr;
		}

		ComPtr<ID3DBlob> shader_binary;
		if ( !SE_ENSURE_HRES( hr = compilation_result->GetOutput( DXC_OUT_OBJECT, IID_PPV_ARGS( shader_binary.GetAddressOf() ), nullptr ) ) )
		{
			std::cerr << "IDxcResult::GetOutput error #" << hr << std::endl;
			return nullptr;
		}

		if ( shader_binary )
		{
			pso_desc.CS =
			{
				reinterpret_cast<BYTE*>( shader_binary->GetBufferPointer() ),
				shader_binary->GetBufferSize()
			};
		}

		if ( FAILED( hr = m_device->CreateComputePipelineState( &pso_desc, IID_PPV_ARGS( &pso ) ) ) )
		{
			std::cerr << "PSO creation error #" << hr << std::endl;
			return nullptr;
		}

		return pso;
	});	

	res = async_res.share();

	m_pso_lut.insert( std::make_pair( CalcHash( request ), res ) );

	return res;
}


namespace
{
	template<typename T> void HashCombine( uint64_t& seed, const T& val )
	{
		seed ^= std::hash<T>()( val ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
	}
	
	template<typename... Types>
	uint64_t HashCombine ( const Types&... args )
	{ 
		uint64_t seed = 0;
		( HashCombine( seed, args ) , ... );
		
		return seed;
	}
}


bool PSOCache::CheckInnerState() const
{
	return SE_ENSURE( m_device )
		&& SE_ENSURE( m_compiler );
}

PSOCache::PsoHash PSOCache::CalcHash( const CompilationRequest& request )
{
	return HashCombine( request.shader_text, request.preprocessor_directives, request.rootsig );
}