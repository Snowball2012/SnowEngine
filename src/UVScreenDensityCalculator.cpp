#include "stdafx.h"

#include "UVScreenDensityCalculator.h"

using namespace DirectX;

UVScreenDensityCalculator::UVScreenDensityCalculator( Scene* scene )
	: m_scene( scene )
{
}


void UVScreenDensityCalculator::Update( CameraID camera )
{

}

void UVScreenDensityCalculator::CalcUVDensityInObjectSpace( StaticSubmesh& submesh )
{
	StaticMeshID mesh_id = submesh.GetMesh();

	assert( m_scene->AllStaticMeshes().has( mesh_id ) );

	const StaticMesh& mesh = m_scene->AllStaticMeshes()[mesh_id];

	if ( ! ( mesh.Indices().size() % 3 == 0 && mesh.Topology() == D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST ) )
		throw SnowEngineException( "only triangle meshes are supported" );

	XMFLOAT2 max_uv_basis_len2( 0.0f, 0.0f );

	const int32_t vertex_offset = submesh.DrawArgs().base_vertex_loc;

	for ( auto i = mesh.Indices().begin() + submesh.DrawArgs().start_index_loc,
		  end = mesh.Indices().begin() + submesh.DrawArgs().start_index_loc + submesh.DrawArgs().idx_cnt;
		  i != end; )
	{
		const Vertex& v1 = mesh.Vertices()[*i++ + vertex_offset];
		const Vertex& v2 = mesh.Vertices()[*i++ + vertex_offset];
		const Vertex& v3 = mesh.Vertices()[*i++ + vertex_offset];

		// Find basis of UV-space on triangle in object space
		// let a = v2 - v1; b = v3 - v1
		//     a_uv = v2.uv - v1.uv; b_uv = v3.uv - v1.uv
		// M = | a_uv | 
		//     | b_uv | 
		// eu, ev - UV-space basis vector coords in object space
		// so
		// | eu | = inv( M ) * | a |
		// | ev |              | b |
		//
		// Inverse uv density is the length of uv basis vectors in object space

		// TODO: use 2x2 specialized code
		XMMATRIX ab( XMVectorSubtract( XMLoadFloat3( &v2.pos ), XMLoadFloat3( &v1.pos ) ),
					 XMVectorSubtract( XMLoadFloat3( &v3.pos ), XMLoadFloat3( &v1.pos ) ),
					 XMVECTORF32{ 0, 0, 1.0f, 0 },
					 XMVECTORF32{ 0, 0, 0, 1.0f } );

		XMMATRIX m_inv( XMVectorSubtract( XMLoadFloat2( &v2.uv ), XMLoadFloat2( &v1.uv ) ),
						XMVectorSubtract( XMLoadFloat2( &v3.uv ), XMLoadFloat2( &v1.uv ) ),
						XMVECTORF32{ 0, 0, 1.0f, 0 },
						XMVECTORF32{ 0, 0, 0, 1.0f } );

		constexpr float determinant_eps = 1.e-5f;

		XMVECTOR det;
		m_inv = XMMatrixInverse( &det, m_inv );
		if ( det.m128_f32[0] < determinant_eps )
			continue;

		XMMATRIX uv_basis = XMMatrixMultiply( m_inv, ab );
		max_uv_basis_len2.x = std::max( XMVector3LengthSq( uv_basis.r[0] ).m128_f32[0], max_uv_basis_len2.x );
		max_uv_basis_len2.y = std::max( XMVector3LengthSq( uv_basis.r[1] ).m128_f32[0], max_uv_basis_len2.y );
	}

	max_uv_basis_len2.x = std::sqrtf( max_uv_basis_len2.x );
	max_uv_basis_len2.y = std::sqrtf( max_uv_basis_len2.y );
	submesh.MaxInverseUVDensity() = max_uv_basis_len2;

}


