#include <boost/test/unit_test.hpp>

#include "../src/Scene.h"

BOOST_AUTO_TEST_SUITE( scene )

BOOST_AUTO_TEST_CASE( creation )
{
	// 1 renderitem : static mesh instance
	Scene scene;

	TransformID tf = scene.AddTransform();

	StaticMeshID mesh = scene.AddStaticMesh();
	StaticSubmeshID submesh = scene.AddStaticSubmesh( mesh );

	TextureID texture1 = scene.AddTexture();
	TextureID texture2 = scene.AddTexture();
	TextureID texture3 = scene.AddTexture();
	MaterialID material = scene.AddMaterial( MaterialPBR::TextureIds{ texture1, texture2, texture3 } );

	MeshInstanceID instance_id = scene.AddStaticMeshInstance( tf, submesh, material );

	const StaticMeshInstance* instance = scene.AllStaticMeshInstances().try_get( instance_id );

	BOOST_TEST( instance != nullptr );
	BOOST_TEST( ( instance->Material() == material ) );
	BOOST_TEST( ( instance->Submesh() == submesh ) );
	BOOST_TEST( ( instance->Transform() == tf ) );
}

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

	MeshInstanceID instance_id;

	Fixture()
	{
		tf = scene.AddTransform();

		mesh = scene.AddStaticMesh();
		submesh = scene.AddStaticSubmesh( mesh );

		texture1 = scene.AddTexture();
		texture2 = scene.AddTexture();
		texture3 = scene.AddTexture();
		material = scene.AddMaterial( MaterialPBR::TextureIds{ texture1, texture2, texture3 } );

		instance_id = scene.AddStaticMeshInstance( tf, submesh, material );
	}
};

BOOST_FIXTURE_TEST_CASE( deletion, Fixture )
{
	// try to remove referenced material
	BOOST_TEST( ! scene.RemoveMaterial( material ) );
	BOOST_TEST( scene.AllMaterials().has( material ) );

	// remove unreferenced scene item
	BOOST_TEST( scene.RemoveStaticMeshInstance( instance_id ) );
	BOOST_TEST( ! scene.AllStaticMeshInstances().has( instance_id ) );

	// now material is unreferenced
	BOOST_TEST( scene.RemoveMaterial( material ) );
	BOOST_TEST( ! scene.AllMaterials().has( material ) );
}

BOOST_AUTO_TEST_SUITE_END()