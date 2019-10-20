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

BOOST_AUTO_TEST_CASE( find_component )
{
    EntityContainer<Position, Velocity> world;
    using Entity = decltype( world )::Entity;

    Entity o1 = world.CreateEntity();
    Entity o2 = world.CreateEntity();

    BOOST_TEST( world.GetComponent<Position>( o2 ) == nullptr );

    world.AddComponent<Position>( o2, Position{ DirectX::XMFLOAT3( 1, 2, 3 ) } );

    Position* pos_comp = world.GetComponent<Position>( o2 );
    BOOST_TEST( pos_comp != nullptr );
    BOOST_TEST( ( ( pos_comp->v.x == 1.0f ) && ( pos_comp->v.y == 2.0f ) && ( pos_comp->v.z == 3.0f ) ) );
    
    BOOST_TEST( world.GetComponent<Position>( o1 ) == nullptr );
}

BOOST_AUTO_TEST_CASE( remove_component )
{
    EntityContainer<Position, Velocity> world;
    using Entity = decltype( world )::Entity;

    Entity o2 = world.CreateEntity();

    BOOST_TEST( world.GetComponent<Position>( o2 ) == nullptr );

    world.AddComponent<Position>( o2, Position{ DirectX::XMFLOAT3( 1, 2, 3 ) } );

    Position* pos_comp = world.GetComponent<Position>( o2 );
    BOOST_TEST( pos_comp != nullptr );
    BOOST_TEST( ( ( pos_comp->v.x == 1.0f ) && ( pos_comp->v.y == 2.0f ) && ( pos_comp->v.z == 3.0f ) ) );
    
    world.RemoveComponent<Position>( o2 );
    BOOST_TEST( world.GetComponent<Position>( o2 ) == nullptr );
}

BOOST_AUTO_TEST_CASE( remove_entity )
{
    EntityContainer<Position, Velocity> world;
    using Entity = decltype( world )::Entity;

    Entity o2 = world.CreateEntity();

    BOOST_TEST( world.GetComponent<Position>( o2 ) == nullptr );

    world.AddComponent<Position>( o2, Position{ DirectX::XMFLOAT3( 1, 2, 3 ) } );

    Position* pos_comp = world.GetComponent<Position>( o2 );
    BOOST_TEST( pos_comp != nullptr );
    BOOST_TEST( ( ( pos_comp->v.x == 1.0f ) && ( pos_comp->v.y == 2.0f ) && ( pos_comp->v.z == 3.0f ) ) );
    
    world.DestroyEntity( o2 );
    BOOST_TEST( world.GetEntityCount() == 0 );
}

BOOST_AUTO_TEST_CASE( iterate_over_view )
{
    EntityContainer<Position, Velocity> world;
    using Entity = decltype( world )::Entity;

    Entity o1 = world.CreateEntity();
    Entity o2 = world.CreateEntity();
    Entity o3 = world.CreateEntity();
    
    world.AddComponent<Position>( o2, Position{ DirectX::XMFLOAT3( 1, 2, 3 ) } );
    world.AddComponent<Velocity>( o1, Velocity{ DirectX::XMFLOAT3( 4, 5, 6 ) } );
    world.AddComponent<Position>( o3, Position{ DirectX::XMFLOAT3( 7, 8, 9 ) } );
    world.AddComponent<Velocity>( o3, Velocity{ DirectX::XMFLOAT3( 10, 11, 12 ) } );

    constexpr float time_step = 0.001f;
    for ( auto& [entity, pos, vel] : world.CreateView<Position, Velocity>() )
    {
        BOOST_TEST( ( entity != o1 && entity != o2 ) );
        pos.v += vel.v * time_step;
    }
}

BOOST_AUTO_TEST_SUITE_END()