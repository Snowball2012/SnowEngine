// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "stdafx.h"

#include "PSOCache.h"


PSOCache::CompilationResult PSOCache::CompilePSO( CompilationRequest request )
{
	// placeholder

	SE_ENSURE( m_device );
	SE_ENSURE( request.rootsig );

	// Calc hash, search for pso in a LUT. Compile a new PSO if not found.
	// ToDo: async compilation

	std::promise<ComPtr<ID3D12PipelineState>> compilation_promise;

	ComPtr<ID3D12PipelineState> pso;

	D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc{};
	pso_desc.pRootSignature = request.rootsig;

	if ( FAILED( m_device->CreateComputePipelineState( &pso_desc, IID_PPV_ARGS( &pso ) ) ) )
		compilation_promise.set_value( nullptr );
	else
		compilation_promise.set_value( pso );

	return CompilationResult( compilation_promise.get_future() );
}
