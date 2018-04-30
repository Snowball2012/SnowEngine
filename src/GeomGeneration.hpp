#pragma once

#include "GeomGeneration.h"
#include "XMMath.h"

namespace GeomGeneration
{
	template<class fnVertexCollector, class fnIndexCollector, class fnUVCollector>
	void MakeGrid( size_t nx, size_t ny, float size_x, float size_y, fnVertexCollector&& vertex_collector, fnIndexCollector&& index_collector, fnUVCollector&& uv_collector )
	{
		const float halfx = size_x / 2;
		const float halfy = size_y / 2;
		for ( size_t j = 0; j < ny; ++j )
			for ( size_t i = 0; i < nx; ++i )
			{
				vertex_collector( float( i ) / float( nx ) * size_x - halfx, float( j ) / float( ny ) * size_y - halfy );
				uv_collector( nx * j + i, DirectX::XMFLOAT2( float( i ) / float( nx - 1 ), float( ny - 1 - j ) / float( ny - 1 ) ) );
			}

		for ( size_t j = 0; j < ny - 1; ++j )
			for ( size_t i = 0; i < nx - 1; ++i )
			{
				// 2 triangles
				const size_t indices[4] =
				{ j * nx + i,       j * nx + i + 1,
				  (j + 1) * nx + i, (j + 1) * nx + i + 1 };

				index_collector( indices[2] );
				index_collector( indices[3] );
				index_collector( indices[0] );

				index_collector( indices[3] );
				index_collector( indices[1] );
				index_collector( indices[0] );
			}
	}

	constexpr std::pair<size_t, size_t> GetGridSize( size_t nx, size_t ny )
	{
		return std::make_pair(nx*ny, GetGridNIndices(nx, ny));
	}

	constexpr size_t GetGridNIndices( size_t nx, size_t ny )
	{
		assert( nx > 0 && ny > 0 );
		return ( nx - 1 )*( ny - 1 ) * 6;
	}

	template<size_t nx, size_t ny>
	std::pair<GridVertices<nx, ny>, GridIndices<nx, ny>> MakeArrayGrid( float size_x, float size_y )
	{
		std::pair<GridVertices<nx, ny>, GridIndices<nx, ny>> res;

		size_t last_vertex = 0, last_idx = 0;

		auto vertex_collector = [&]( float x, float z )
		{
			auto& vertex = res.first[last_vertex++];
			vertex.pos = DirectX::XMFLOAT3( x, 0, z );
		};

		auto index_collector = [&]( size_t idx )
		{
			res.second[last_idx++] = uint16_t( idx );
		};

		auto uv_collector = [&]( size_t idx, const DirectX::XMFLOAT2& uv )
		{
			res.first[idx].uv = uv;
		};

		MakeGrid( nx, ny, size_x, size_y, vertex_collector, index_collector, uv_collector );

		return res;
	}

	template<class VertexPosRandomAccessRange, class IndexRandomAccessRange, class fnNormalCollector>
	void CalcAverageNormals( const IndexRandomAccessRange& indices, const VertexPosRandomAccessRange& vertices, fnNormalCollector&& nc )
	{
		static_assert( std::is_same_v<std::decay_t<decltype( vertices[0] )>, XMFLOAT3>, "vertex position must be XMFLOAT3" );

		for ( const auto idx : indices )
			nc( idx ) = XMFLOAT3( 0, 0, 0 );

		for ( size_t i = 2; i < indices.size(); i += 3 )
		{
			XMVECTOR i_pos = XMLoadFloat3( &vertices[indices[i]] );
			// clockwise triangles
			XMFLOAT3 normal;
			XMStoreFloat3( &normal, XMVector3Normalize( XMVector3Cross( XMLoadFloat3( &vertices[indices[i - 2]] ) - i_pos,
																		XMLoadFloat3( &vertices[indices[i - 1]] ) - i_pos ) ) );
			
			for ( size_t j = 0; j < 3; ++j )
				nc( indices[i - j] ) += normal;
		}

		for ( const auto idx : indices )
			XMFloat3Normalize( nc( idx ) );
	}
}