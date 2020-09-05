// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#pragma once

#include "CommonTypedefs.h"

#include <d3d12.h>

#include <future>
#include <string>


// Uses shader text hashes. Beware of collisions
class PSOCache
{
public:
	using PsoId = uint64_t;
	struct CompilationRequest
	{
		std::string shader_text;
		ID3D12RootSignature* rootsig;
	};

	using CompilationResult = std::shared_future<ComPtr<ID3D12PipelineState>>;

private:
	using PsoHash = uint64_t;
	std::unordered_map<PsoHash, CompilationResult> m_pso_lut;
	ComPtr<ID3D12Device> m_device;

public:
	CompilationResult CompilePSO( CompilationRequest request );

private:
};
