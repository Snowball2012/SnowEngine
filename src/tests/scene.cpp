// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <boost/test/unit_test.hpp>

#include <snow_engine/stdafx.h>
#include <snow_engine/Scene.h>

BOOST_AUTO_TEST_SUITE( scene )

// TODO: add relevant tests after scene rework

struct Fixture
{
	Scene scene;

	TransformID tf;

	StaticMeshID mesh;
	StaticSubmeshID submesh;

	TextureID texture1;
	TextureID texture2;
	TextureID texture3;
	MaterialID material;


	Fixture()
	{
		tf = scene.AddTransform();

		mesh = scene.AddStaticMesh();
		submesh = scene.AddStaticSubmesh( mesh );

		texture1 = scene.AddTexture();
		texture2 = scene.AddTexture();
		texture3 = scene.AddTexture();
		material = scene.AddMaterial( MaterialPBR::TextureIds{ texture1, texture2, texture3 } );

	}
};

BOOST_FIXTURE_TEST_CASE( deletion, Fixture )
{
}

BOOST_AUTO_TEST_SUITE_END()