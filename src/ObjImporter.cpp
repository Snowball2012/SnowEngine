#include "stdafx.h"

#include "ObjImporter.h"

MeshData ObjImporter::ParseObj( std::istream& input ) const
{
	std::vector<Vertex> vertices;
	std::vector<uint16_t> faces;

	std::string first_word;
	while ( input >> first_word )
	{
		if ( first_word == "v" )
		{
			vertices.emplace_back();
			auto& new_vertex = vertices.back();
			input >> new_vertex.pos.x >> new_vertex.pos.y >> new_vertex.pos.z;
		}
		else if ( first_word == "f" )
		{
			uint16_t start_idx = uint16_t( faces.size() );
			faces.resize( start_idx + 3 );
			for ( int i : { 0, 1, 2 } )
			{
				input >> faces[start_idx + i];
				faces[start_idx + i]--;
				input >> std::noskipws;
				char c;
				do
				{
					input >> c;
				} while ( c != '\n' && c != ' ' && !input.eof() );
				input >> std::skipws;
			}
		}
		else
		{
			// not supported yet
			std::string rest;
			std::getline( input, rest );
		}
	}

	return { std::move( vertices ), std::move( faces ) };
}
