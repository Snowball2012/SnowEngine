// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <boost/test/unit_test.hpp>

#include <utils/MathUtils.h>

BOOST_AUTO_TEST_SUITE( intersections )

BOOST_AUTO_TEST_CASE( ray_triangle_simple )
{
	// 1. Hit on the first vertex
	{
		float triangle[9] = 
		{
			0, 0, 0,
			1, 0, 0,
			0, 1, 0
		};

		float ray_origin[3] = { 0, 0, -1 };
		float ray_dir[3] = { 0, 0, 1 };

		auto intersection_result = IntersectRayTriangle( triangle, ray_origin, ray_dir );
		float tolerance = std::sqrt( FLT_EPSILON );

		BOOST_TEST( intersection_result.HitDetected( tolerance, tolerance, tolerance ) == true );

		BOOST_TEST( intersection_result.RayEntersTriangle() == true );
		BOOST_TEST( intersection_result.IsDegenerate( tolerance ) == false );
		BOOST_CHECK_CLOSE( intersection_result.coords.m128_f32[0], 1.0f, tolerance );
		BOOST_CHECK_CLOSE( intersection_result.coords.m128_f32[1], 0.0f, tolerance );
	}

	// 2. Ray on a plane
	{
		float triangle[9] = 
		{
			0, 0, 0,
			1, 0, 0,
			0, 1, 0
		};

		float ray_origin[3] = { 0, 0, -1 };
		float ray_dir[3] = { 1, 0, 0 };

		auto intersection_result = IntersectRayTriangle( triangle, ray_origin, ray_dir );
		float tolerance = std::sqrt( FLT_EPSILON );

		BOOST_TEST( intersection_result.HitDetected( tolerance, tolerance, tolerance ) == false );
		BOOST_TEST( intersection_result.IsDegenerate( tolerance ) == true );
	}

	// 3. Generic case
	{
		float triangle[9] = 
		{
			0, 0, 0,
			1, 0, 0,
			0, 1, 0
		};

		float ray_origin[3] = { 0, 0, -1 };
		float ray_dir[3] = { 1, 0, 0 };

		auto intersection_result = IntersectRayTriangle( triangle, ray_origin, ray_dir );
		float tolerance = std::sqrt( FLT_EPSILON );

		BOOST_TEST( intersection_result.HitDetected( tolerance, tolerance, tolerance ) == false );
		BOOST_TEST( intersection_result.IsDegenerate( tolerance ) == true );
	}
}

BOOST_AUTO_TEST_SUITE_END()