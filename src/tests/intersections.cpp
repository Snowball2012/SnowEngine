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

		auto intersection_result = IntersectRayTriangle( triangle, triangle + 3, triangle + 6, ray_origin, ray_dir );
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

		auto intersection_result = IntersectRayTriangle( triangle, triangle + 3, triangle + 6, ray_origin, ray_dir );
		float tolerance = std::sqrt( FLT_EPSILON );

		BOOST_TEST( intersection_result.HitDetected( tolerance, tolerance, tolerance ) == false );
		BOOST_TEST( intersection_result.IsDegenerate( tolerance ) == true );
	}
}


BOOST_AUTO_TEST_CASE( ray_triangle_regression )
{
	// 1. Miss ( u and v in [0, 1] range, w is outside )
	{
		float triangle[9] = 
		{
			483.263489, -8.27392387, 115.925697,
			482.492981, -9.06385040, 117.638611,
			480.742371, -8.40032768, 117.098801
		};

		float ray_origin[3] = { 1350.00000, 273.000000, 413.000000 };
		float ray_dir[3] = { -90.4508514, -29.3892651, -30.9016972 };

		auto intersection_result = IntersectRayTriangle( triangle, triangle + 3, triangle + 6, ray_origin, ray_dir );
		float tolerance = std::sqrt( FLT_EPSILON );

		BOOST_TEST( intersection_result.HitDetected( tolerance, tolerance, tolerance ) == false );
		BOOST_TEST( intersection_result.IsDegenerate( tolerance ) == false );
	}

	// 2. Hit with 1.e-3 precision
	{
		float triangle[9] = 
		{
			421.495972, -84.4974136, 88.5863800,
			421.811707, -84.7294006, 88.3023300,
			416.407593, -91.5232239, 87.8444443
		};

		float ray_origin[3] = { 1350.00000, 273.000000, 413.000000 };
		float ray_dir[3] = { -88.6392212, -34.4699135, -30.9016972 };

		auto intersection_result = IntersectRayTriangle( triangle, triangle + 3, triangle + 6, ray_origin, ray_dir );
		float hit_tolerance = std::sqrt( FLT_EPSILON );
		float target_precision = 1.e-3f;

		BOOST_TEST( intersection_result.HitDetected( hit_tolerance, hit_tolerance, hit_tolerance ) == true );
		BOOST_TEST( intersection_result.Validate( triangle, triangle + 3, triangle + 6, ray_origin, ray_dir, target_precision ) == true );
	}
	
	// 3. Hit with 1.5e-3 precision
	{
		float triangle[9] = 
		{
			520.199768, 138.087601, 112.856644,
			520.107422, 139.128647, 112.856598,
			520.108337, 139.128616, 22.4655190
		};

		float ray_origin[3] = { 1230.73145, 299.535034, 475.945282 };
		float ray_dir[3] = { -85.7147598, -19.3952808, -47.7158546 };

		auto intersection_result = IntersectRayTriangle( triangle, triangle + 3, triangle + 6, ray_origin, ray_dir );
		float hit_tolerance = std::sqrt( FLT_EPSILON );
		float target_precision = 1.5e-3f;

		BOOST_TEST( intersection_result.HitDetected( hit_tolerance, hit_tolerance, hit_tolerance ) == true );
		BOOST_TEST( intersection_result.Validate( triangle, triangle + 3, triangle + 6, ray_origin, ray_dir, target_precision ) == true );
	}
}

BOOST_AUTO_TEST_SUITE_END()