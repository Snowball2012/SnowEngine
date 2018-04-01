#pragma once

#include <array>
#include <cetonia/parser.h>
#include "ObjImporter.h"

namespace GeomGeneration
{
	constexpr std::array<uint16_t, 6> line_indices =
	{
		3, 2, 0,
		1, 3, 0
	};

	void MakeLineVertices( const ctTokenLine2d& line, Vertex vertices[4] );

	// clockwise triangles
	template<class fnVertexCollector, class fnIndexCollector>
	void MakeGrid( size_t nx, size_t ny, float size_x, float size_y, fnVertexCollector&& vc, fnIndexCollector&& ic );

	constexpr std::pair<size_t/*nvertices*/, size_t/*nindices*/> GetGridSize( size_t nx, size_t ny ); // may nor work on MSVC17
	constexpr size_t GetGridNIndices( size_t nx, size_t ny );

	template<size_t nx, size_t ny>
	using GridVertices = std::array<Vertex, nx * ny>;
	template<size_t nx, size_t ny>
	using GridIndices = std::array<uint16_t, GetGridNIndices( nx, ny )>;

	// specialization of MakeGrid for static array
	template<size_t nx, size_t ny>
	std::pair<GridVertices<nx, ny>, GridIndices<nx, ny>> MakeArrayGrid( float size_x, float size_y );
}

#include "GeomGeneration.hpp"