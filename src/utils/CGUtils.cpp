// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "CGUtils.h"


uint32_t CalcMipNumber( uint32_t resolution )
{
    
    return _mm_popcnt_u32(upper_power_of_two( resolution ) - 1) + 1;
}


bool FindPSSMFrustumsForItem(
	const DirectX::XMVECTOR& camera_to_item_center, float item_radius,
	const DirectX::XMVECTOR& light_dir, const span<const float>& split_distances,
	span<bool>& containing_splits)
{
	assert( split_distances.size() + 1 == containing_splits.size() );

	// simple FLT_EPSILON makes no sense here because we compare length to 1.0f
	// and flt_eps is the smallest difference between 1 and anything else
	assert( is_close( DirectX::XMVectorGetX(DirectX::XMVector3LengthSq( light_dir ) ), 1, std::sqrt( FLT_EPSILON ) ) );

	item_radius *= 1.001f;
	item_radius += 1.e-3f;

	const DirectX::XMVECTOR item_to_axis =
		DirectX::XMVectorNegativeMultiplySubtract(
			light_dir, DirectX::XMVector3Dot( camera_to_item_center, light_dir ),
			camera_to_item_center);

	const float center_to_axis_dist = DirectX::XMVectorGetX( DirectX::XMVector3LengthEst( item_to_axis ) );

	const float min_dist = std::max( 0.0f, center_to_axis_dist - item_radius );
	const float max_dist = center_to_axis_dist + item_radius;

	bool completely_in_split = false;
	for ( size_t i = 0; i < split_distances.size(); ++i )
	{
		const float split_pos = split_distances[i];
		if ( min_dist < split_pos )
			containing_splits[i] = true;
		if ( max_dist <= split_pos )
		{
			completely_in_split = true;
			break;
		}
	}

	if ( ! completely_in_split )
	{
		// add to the last split
		containing_splits.back() = true;
	}

	return true;
}
