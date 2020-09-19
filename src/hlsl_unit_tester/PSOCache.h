// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#pragma once

#include "CommonTypedefs.h"

#include <atlbase.h>
#include <wrl.h>
#include <WindowsX.h>

#include <future>
#include <string>

// Uses shader text hashes. Beware of collisions
class PSOCache
{
public:

	struct CompilationRequest
	{
		std::string shader_text;
		std::string preprocessor_directives; // an argument to dxc
		ID3D12RootSignature* rootsig;
	};

	using CompilationResult = std::shared_future<ComPtr<ID3D12PipelineState>>;

private:

	using PsoHash = uint64_t;
	std::unordered_map<PsoHash, CompilationResult> m_pso_lut;

	ComPtr<ID3D12Device> m_device;
	CComPtr<IDxcCompiler3> m_compiler;
	CComPtr<IDxcIncludeHandler> m_include_handler;

public:

	PSOCache(
		ComPtr<ID3D12Device> device, CComPtr<IDxcCompiler3> compiler,
		CComPtr<IDxcIncludeHandler> include_handler );

	CompilationResult CompilePSO( CompilationRequest request );

private:

	bool CheckInnerState() const; // Checks class invariants. Use in all public methods before doing anything else

	static PsoHash CalcHash( const CompilationRequest& request );
};
