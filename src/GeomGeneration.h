#pragma once

#include <array>
#include <cetonia/parser.h>
#include "SceneImporter.h"

namespace GeomGeneration
{
	constexpr std::array<uint16_t, 6> line_indices =
	{
		3, 2, 0,
		1, 3, 0
	};

	void MakeLineVertices( const ctTokenLine2d& line, Vertex vertices[4] );

	// clockwise triangles
	template<class fnVertexCollector, class fnIndexCollector, class fnUVCollector>
	void MakeGrid( size_t nx, size_t ny, float size_x, float size_y, fnVertexCollector&& vc, fnIndexCollector&& ic, fnUVCollector&& uvc );

	constexpr std::pair<size_t/*nvertices*/, size_t/*nindices*/> GetGridSize( size_t nx, size_t ny ); // may nor work on MSVC17
	constexpr size_t GetGridNIndices( size_t nx, size_t ny );

	template<size_t nx, size_t ny>
	using GridVertices = std::array<Vertex, nx * ny>;
	template<size_t nx, size_t ny>
	using GridIndices = std::array<uint16_t, GetGridNIndices( nx, ny )>;

	// specialization of MakeGrid for static array
	template<size_t nx, size_t ny>
	std::pair<GridVertices<nx, ny>, GridIndices<nx, ny>> MakeArrayGrid( float size_x, float size_y );

	// fnNormalCollector = []( size_t vertex_idx ) -> XMFLOAT3&
	// unsafe, every index in indices must be < size(vertices)
	template<class VertexPosRandomAccessRange, class IndexRandomAccessRange, class fnNormalCollector>
	void CalcAverageNormals( const IndexRandomAccessRange& indices, const VertexPosRandomAccessRange& vertices, fnNormalCollector&& nc );
}

#include "GeomGeneration.hpp"