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

void ParallelSplitShadowMapping::CalcSplitPositionsVS( float near_z, float far_z, uint32_t splits_cnt, float uniform_factor, float positions[] ) noexcept
{
	assert( ( far_z > 0 ) && ( near_z > 0 ) );
	assert( far_z > near_z );
	assert( uniform_factor >= 0 && uniform_factor <= 1 );
	assert( positions != nullptr );
	assert( splits_cnt > 0 );

	// calc logarithmic split and uniform split, interpolate between them

	for ( uint32_t i = 1; i < splits_cnt; ++i )
	{
		const float si = float( i ) / float( splits_cnt );
		positions[i - 1] = lerp( LogarithmicSplit( near_z, far_z, si ),
								 UniformSplit( near_z, far_z, si ),
								 uniform_factor );
	}
}
