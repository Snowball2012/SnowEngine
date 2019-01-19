#pragma once

#include <d3d12.h>
#include <boost/container/small_vector.hpp>

#include "Ptr.h"

#include "utils/packed_freelist.h"

// This class incapsulates render pass with fixed render state
//
// Typical use scenario:
// 1. Derive from it
// 2. Create neccessary render states with a derived class
// 3. Begin( state, cmd_list )
// 4. Record some commands with a derived class
// 5. End()

class RenderPass
{
protected:
	using RenderState = ComPtr<ID3D12PipelineState>;

	template<class T>
	using PSOFreelistStorage = boost::container::small_vector<T, 1>;

	using RenderStates = packed_freelist<RenderState, PSOFreelistStorage>;

public:
	using RenderStateID = typename RenderStates::id;

	void Begin( RenderStateID state, ID3D12GraphicsCommandList& command_list ) noexcept;
	void End() noexcept;

	void DeleteState( RenderStateID state ) noexcept;

protected:

	virtual void BeginDerived( RenderStateID state ) noexcept = 0;

	ID3D12GraphicsCommandList* m_cmd_list = nullptr;

	RenderStates m_pso_cache;
};