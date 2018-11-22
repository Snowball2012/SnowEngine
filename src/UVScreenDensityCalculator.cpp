#include "stdafx.h"

#include "UVScreenDensityCalculator.h"


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
	const StaticMesh& mesh = m_scene->AllStaticMeshes()[mesh_id];

	if ( ! mesh.IsLoaded() )
		throw SnowEngineException( "mesh has not been loaded yet" );

	if ( ! ( mesh.Indices().size() % 3 == 0 && mesh.Topology() == D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST ) )
		throw SnowEngineException( "only triangle meshes are supported" );

	for ( auto i = mesh.Indices().begin(), end = mesh.Indices().end(); i != end; )
	{
		const Vertex& v1 = mesh.Vertices()[*i++];
		const Vertex& v2 = mesh.Vertices()[*i++];
		const Vertex& v3 = mesh.Vertices()[*i++];

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

		NOTIMPL;
	}
}


