#include <boost/test/unit_test.hpp>

#include "../src/ParallelSplitShadowMapping.h"

BOOST_AUTO_TEST_SUITE( pssm )

BOOST_AUTO_TEST_CASE( split_pos_calc )
{
	// 1 renderitem : static mesh instance
	std::array<float, 2> split_positions;

	const float n = 0.01f;
	const float f = 100.0f;

	const float uniform_factor = 0.5f;

	ParallelSplitShadowMapping::CalcSplitPositionsVS( n, f, split_positions.size() + 1, uniform_factor, split_positions.data() );

	BOOST_CHECK_CLOSE( split_positions[0], 16.777, 1 );
}

BOOST_AUTO_TEST_SUITE_END()