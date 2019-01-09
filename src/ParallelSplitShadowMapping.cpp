// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "ParallelSplitShadowMapping.h"

namespace
{
	// for the next 2 functions si is normalized shadow plane position (gpugems 3, chapter 10)
	float LogarithmicSplit( float near_z, float far_z, float si )
	{
		assert( near_z > 0 );
		return near_z * std::pow( far_z / near_z, si );
	}

	float UniformSplit( float near_z, float far_z, float si )
	{
		return lerp( near_z, far_z, si );
	}
}


void ParallelSplitShadowMapping::CalcSplitPositionsVS( float near_z, float far_z, float uniform_factor, span<float> positions_storage ) noexcept
{
	assert( ( far_z > 0 ) && ( near_z > 0 ) );
	assert( far_z > near_z );
	assert( uniform_factor >= 0 && uniform_factor <= 1 );
	assert( positions_storage.cbegin() != nullptr );

	// calc logarithmic split and uniform split, interpolate between them

	const uint32_t splits_num = uint32_t( positions_storage.size() + 1 );
	for ( uint32_t i = 1; i < splits_num; ++i )
	{
		const float si = float( i ) / float( splits_num );
		positions_storage[i - 1] = lerp( LogarithmicSplit( near_z, far_z, si ),
								         UniformSplit( near_z, far_z, si ),
								         uniform_factor );
	}
}


span<float> ParallelSplitShadowMapping::CalcSplitPositionsVS( const Camera::Data& camera, span<float> positions_storage ) const noexcept
{
	assert( positions_storage.size() >= m_splits_num - 1 );

	span<float> res_span( positions_storage.begin(), positions_storage.begin() + m_splits_num - 1 );
	CalcSplitPositionsVS( camera.near_plane, camera.far_plane, m_uniform_factor, res_span );

	return res_span;
}


span<DirectX::XMFLOAT4X4> ParallelSplitShadowMapping::CalcShadowMatricesWS( const Camera& camera, const SceneLight& light, const span<float>& split_positions, span<DirectX::XMFLOAT4X4> matrices_storage ) const noexcept
{
	NOTIMPL;
	return span<DirectX::XMFLOAT4X4>();
}


void ParallelSplitShadowMapping::SetUniformFactor( float uniform_factor ) noexcept
{
	assert( uniform_factor >= 0 && uniform_factor <= 1 );
	m_uniform_factor = uniform_factor;
}

