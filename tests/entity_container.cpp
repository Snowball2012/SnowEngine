#include <boost/test/unit_test.hpp>

#include "../src/EntityContainer.h"

#include "../src/MathUtils.h"


BOOST_AUTO_TEST_SUITE( entity_container )

struct Position
{
    DirectX::XMFLOAT3 v;
};

struct Velocity
{
    DirectX::XMFLOAT3 v;
};

BOOST_AUTO_TEST_CASE( creation )
{
    EntityContainer<Position, Velocity> world;
    using Entity = decltype( world )::Entity;
    Entity test_obj = world.CreateEntity();

    BOOST_TEST( world.GetEntityCount() == 1 );

    auto& pos = world.AddComponent<Position>( test_obj, Position{ DirectX::XMFLOAT3( 1, 2, 3 ) } );
    BOOST_TEST( ( ( pos.v.x == 1.0f ) && ( pos.v.y == 2.0f ) && ( pos.v.z == 3.0f ) ) );
}

BOOST_AUTO_TEST_SUITE_END()