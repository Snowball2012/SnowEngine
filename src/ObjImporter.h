#pragma once

#include <DirectXMath.h>

struct Vertex
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT4 color;
};

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