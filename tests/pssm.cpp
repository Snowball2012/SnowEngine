// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <boost/test/unit_test.hpp>

#include <Windows.h>
#include <DirectXMath.h>

#include "../src/ParallelSplitShadowMapping.h"

BOOST_AUTO_TEST_SUITE( pssm )

BOOST_AUTO_TEST_CASE( split_pos_calc )
{
	// 1 renderitem : static mesh instance
	std::array<float, 2> split_positions;

	const float n = 0.01f;
	const float f = 100.0f;

	const float uniform_factor = 0.5f;

	ParallelSplitShadowMapping::CalcSplitPositionsVS( n, f, uniform_factor, span<float>( split_positions.data(), split_positions.data() + split_positions.size() ) );

	BOOST_CHECK_CLOSE( split_positions[0], 16.777, 1 );
}

BOOST_AUTO_TEST_SUITE_END()