// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "ParallelSplitShadowMapping.h"

using namespace DirectX;

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


void ParallelSplitShadowMapping::CalcShadowMatrixForFrustrumLH( const span<XMVECTOR>& frustrum_vertices,
																const XMVECTOR& light_dir, float additional_height,
																XMMATRIX& shadow_matrix ) noexcept
{
	assert( frustrum_vertices.size() == 8 );

	// Try 2 different box orientations, pick the one with minimal side

	DirectX::XMVECTOR up;
	if ( std::abs( DirectX::XMVectorGetY( light_dir ) ) < 0.9 )
		up = DirectX::XMVector3Cross( light_dir, DirectX::XMVectorSet( 0, 1, 0, 0 ) );
	else
		up = DirectX::XMVector3Cross( light_dir, DirectX::XMVectorSet( 1, 0, 0, 0 ) );
	up = DirectX::XMVector3Normalize( up );

	DirectX::XMMATRIX light_space[2];
	light_space[0] = DirectX::XMMatrixLookToLH( DirectX::XMVectorZero(), DirectX::XMVectorNegate( light_dir ), up );
	light_space[1] = DirectX::XMMatrixLookToLH( DirectX::XMVectorZero(), DirectX::XMVectorNegate( light_dir ), DirectX::XMVector3Transform( up, DirectX::XMMatrixRotationAxis( light_dir, DirectX::XM_PIDIV4 ) ) );;

	DirectX::XMVECTOR min_coords[2];
	DirectX::XMVECTOR max_coords[2];
	DirectX::XMVECTOR ls_size[2];
	float max_side[2];

	for ( int i = 0; i < 2; ++i )
	{
		min_coords[i] = max_coords[i] = DirectX::XMVector3Transform( frustrum_vertices[0], light_space[i] );

		for ( int j = 1; j < frustrum_vertices.size(); j++ )
		{
			DirectX::XMVECTOR vertex_ls = DirectX::XMVector3Transform( frustrum_vertices[j], light_space[i] );
			min_coords[i] = DirectX::XMVectorMin( min_coords[i], vertex_ls );
			max_coords[i] = DirectX::XMVectorMax( max_coords[i], vertex_ls );
		}
		ls_size[i] = DirectX::XMVectorSubtract( max_coords[i], min_coords[i] );
		max_side[i] = std::max( DirectX::XMVectorGetX( ls_size[i] ), DirectX::XMVectorGetY( ls_size[i] ) );
	}

	int min_i = max_side[0] < max_side[1] ? 0 : 1;

	const float box_depth = DirectX::XMVectorGetZ( ls_size[min_i] );
	DirectX::XMVECTOR box_center = DirectX::XMVectorScale( DirectX::XMVectorAdd( min_coords[min_i], max_coords[min_i] ), 0.5f );

	DirectX::XMVECTOR light_space_det;
	DirectX::XMMATRIX light_space_inv = DirectX::XMMatrixInverse( &light_space_det, light_space[min_i] );
	box_center = DirectX::XMVector4Transform( box_center, light_space_inv );

	DirectX::XMVECTOR light_eye = DirectX::XMVectorScale( light_dir, box_depth / 2.0f + additional_height );
	light_eye = DirectX::XMVectorAdd( light_eye, box_center );

	light_space[min_i] = light_space[min_i] * DirectX::XMMatrixTranslationFromVector( light_eye );

	light_space[min_i] = ( min_i == 0 ) ? DirectX::XMMatrixLookToLH( light_eye, DirectX::XMVectorNegate( light_dir ), up )
											: DirectX::XMMatrixLookToLH( light_eye, DirectX::XMVectorNegate( light_dir ), DirectX::XMVector3Transform( up, DirectX::XMMatrixRotationAxis( light_dir, DirectX::XM_PIDIV4 ) ) );

	light_space[min_i] = light_space[min_i] * DirectX::XMMatrixOrthographicLH( max_side[min_i],
	                                                                           max_side[min_i],
	                                                                           0,
	                                                                           box_depth + additional_height );

	shadow_matrix = light_space[min_i];

	DirectX::XMVECTOR dbg = DirectX::XMVector4Transform( frustrum_vertices[0], shadow_matrix );
	dbg = DirectX::XMVector4Transform( frustrum_vertices[1], shadow_matrix );
	dbg = DirectX::XMVector4Transform( frustrum_vertices[2], shadow_matrix );
	dbg = DirectX::XMVector4Transform( frustrum_vertices[3], shadow_matrix );
	dbg = DirectX::XMVector4Transform( frustrum_vertices[4], shadow_matrix );
	dbg = DirectX::XMVector4Transform( frustrum_vertices[5], shadow_matrix );
	dbg = DirectX::XMVector4Transform( frustrum_vertices[6], shadow_matrix );
	dbg = DirectX::XMVector4Transform( frustrum_vertices[7], shadow_matrix );

	bool wrk = true;
}


span<float> ParallelSplitShadowMapping::CalcSplitPositionsVS( const Camera::Data& camera, span<float> positions_storage ) const noexcept
{
	assert( positions_storage.size() >= m_splits_num - 1 );

	span<float> res_span( positions_storage.begin(), positions_storage.begin() + m_splits_num - 1 );
	CalcSplitPositionsVS( camera.near_plane, camera.far_plane, m_uniform_factor, res_span );

	return res_span;
}

namespace
{
	DirectX::XMVECTOR NormalizeByW( const DirectX::XMVECTOR& v )
	{
		return DirectX::XMVectorDivide( v, DirectX::XMVectorSplatW( v ) );
	}
}

span<DirectX::XMMATRIX> ParallelSplitShadowMapping::CalcShadowMatricesWS( const Camera::Data& camera, const SceneLight& light, const span<float>& split_positions, span<XMMATRIX> matrices_storage ) const
{
	assert( camera.type == Camera::Type::Perspective );
	assert( light.GetData().type == SceneLight::LightType::Parallel );
	assert( light.GetShadow().has_value() );

	const auto& shadow_desc = *light.GetShadow();

	if ( shadow_desc.num_cascades > split_positions.size() + 1 )
		throw SnowEngineException( "light has too much cascades" );

	if ( matrices_storage.size() < shadow_desc.num_cascades )
		throw SnowEngineException( "matrices storage is too small" );

	float last_z = camera.near_plane;

	DirectX::XMVECTOR frustrum_vertices[8];
	DirectX::XMVECTOR light_dir = XMLoadFloat3( &light.GetData().dir );

	for ( uint32_t i = 0; i < shadow_desc.num_cascades; ++i )
	{
		const float new_z = ( i < shadow_desc.num_cascades - 1 ) ? split_positions[i] : camera.far_plane;

		DirectX::XMMATRIX frustrum_matrix = DirectX::XMMatrixLookToLH( XMLoadFloat3( &camera.pos ), XMLoadFloat3( &camera.dir ), XMLoadFloat3( &camera.up ) )
			                                * DirectX::XMMatrixPerspectiveFovLH( camera.fov_y, camera.aspect_ratio, last_z, new_z );
		last_z = new_z;
		DirectX::XMVECTOR det;
		frustrum_matrix = DirectX::XMMatrixInverse( &det, frustrum_matrix );

		if ( i == 0 )
		{
			frustrum_vertices[0] = NormalizeByW( DirectX::XMVector4Transform( DirectX::XMVectorSet( -1, -1, 0, 1 ), frustrum_matrix ) );
			frustrum_vertices[1] = NormalizeByW( DirectX::XMVector4Transform( DirectX::XMVectorSet( -1,  1, 0, 1 ), frustrum_matrix ) );
			frustrum_vertices[2] = NormalizeByW( DirectX::XMVector4Transform( DirectX::XMVectorSet(  1, -1, 0, 1 ), frustrum_matrix ) );
			frustrum_vertices[3] = NormalizeByW( DirectX::XMVector4Transform( DirectX::XMVectorSet(  1,  1, 0, 1 ), frustrum_matrix ) );
		}

		frustrum_vertices[4] = NormalizeByW( DirectX::XMVector4Transform( DirectX::XMVectorSet( -1, -1, 1, 1 ), frustrum_matrix ) );
		frustrum_vertices[5] = NormalizeByW( DirectX::XMVector4Transform( DirectX::XMVectorSet( -1,  1, 1, 1 ), frustrum_matrix ) );
		frustrum_vertices[6] = NormalizeByW( DirectX::XMVector4Transform( DirectX::XMVectorSet(  1, -1, 1, 1 ), frustrum_matrix ) );
		frustrum_vertices[7] = NormalizeByW( DirectX::XMVector4Transform( DirectX::XMVectorSet(  1,  1, 1, 1 ), frustrum_matrix ) );

		CalcShadowMatrixForFrustrumLH( make_span( frustrum_vertices ),
									   light_dir,
									   shadow_desc.orthogonal_ws_height,
									   matrices_storage[i] );

		for ( uint32_t j = 0; j < 4; ++j )
			frustrum_vertices[j] = frustrum_vertices[j + 4];
	}

	return make_span( matrices_storage );
}


void ParallelSplitShadowMapping::SetUniformFactor( float uniform_factor ) noexcept
{
	assert( uniform_factor >= 0 && uniform_factor <= 1 );
	m_uniform_factor = uniform_factor;
}

