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


span<float> ParallelSplitShadowMapping::CalcSplitPositionsVS( const Camera::Data& camera, span<float> positions_storage ) const noexcept
{
	assert( positions_storage.size() >= m_splits_num - 1 );

	span<float> res_span( positions_storage.begin(), positions_storage.begin() + m_splits_num - 1 );
	CalcSplitPositionsVS( camera.near_plane, camera.far_plane, m_uniform_factor, res_span );

	return res_span;
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

	XMVECTOR camera_pos = XMLoadFloat3( &camera.pos );
	XMVECTOR camera_dir = XMLoadFloat3( &camera.dir );
	XMVECTOR dir = XMLoadFloat3( &light.GetData().dir );
	XMVECTOR up = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );

	XMMATRIX view;
	XMMATRIX proj;

	for ( uint32_t i = 0; i < shadow_desc.num_cascades; ++i )
	{
		XMVECTOR target = camera_pos + camera_dir * ( i > 0 ? ( split_positions[i - 1]) : 0 );
		XMVECTOR pos = dir * shadow_desc.orthogonal_ws_height + target;

		view = XMMatrixLookAtLH( pos, target, up );

		proj = XMMatrixOrthographicLH( shadow_desc.ws_halfwidth * (i + 1),
										shadow_desc.ws_halfwidth * (i + 1),
										shadow_desc.orthogonal_ws_height * 0.1f,
										shadow_desc.orthogonal_ws_height * 2.0f );

		matrices_storage[i] = view * proj;
	}

	return make_span( matrices_storage.begin(), matrices_storage.begin() + shadow_desc.num_cascades );
}


void ParallelSplitShadowMapping::SetUniformFactor( float uniform_factor ) noexcept
{
	assert( uniform_factor >= 0 && uniform_factor <= 1 );
	m_uniform_factor = uniform_factor;
}

