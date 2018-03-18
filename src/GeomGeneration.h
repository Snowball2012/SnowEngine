#pragma once

#include <array>
#include <cetonia/parser.h>
#include "ObjImporter.h"

namespace GeomGeneration
{
	const std::array<uint16_t, 6> line_indices =
	{
		3, 2, 0,
		1, 3, 0
	};

	void MakeLineVertices( const ctTokenLine2d& line, Vertex vertices[4] );
}