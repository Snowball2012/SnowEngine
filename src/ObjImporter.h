#pragma once

#include <DirectXMath.h>
#include "RenderData.h"

struct MeshData
{
	std::vector<Vertex> m_vertices;
	std::vector<uint16_t> m_faces;
};

class ObjImporter
{
public:
	MeshData ParseObj( std::istream& input ) const;
};