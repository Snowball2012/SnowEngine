#pragma once

#include <boost/container/flat_map.hpp>

#include "utils/btree.h"
#include "utils/packed_freelist.h"


namespace details
{
    template<typename EC>
    struct ECSBTreeCallback
    {
        ECSBTreeCallback( EC& c ) : container( c ) {}
        template<typename E, typename T>
        void operator()( const E& entity, const T& new_cursor ) const;

        EC& container;
    };
}


template<typename ... Components>
class EntityContainer
{
    struct alignas(void*) CursorPlaceholder // terrible hack, replace it with an actual btree cursor 
    {
        char data[12];
    };

    template<typename Component>
    static constexpr uint32_t CalcBTreeFactor( uint32_t memblock_size )
    {
        return 10; // ToDo: actual btree factor calculation
    }

    using Entity2Components =
        packed_freelist<
            boost::container::flat_map<
                size_t /*component_typeid*/,
                CursorPlaceholder /*component_cursor*/
            >
        >;

public:
    using Entity = typename Entity2Components::id;

    // Noncopyable, nonmovable
    EntityContainer();
    EntityContainer( const EntityContainer& ) = delete;
    EntityContainer( EntityContainer&& ) = delete;
    EntityContainer& operator=( const EntityContainer& ) = delete;
    EntityContainer& operator=( EntityContainer&& ) = delete;

    Entity CreateEntity();

    template<typename Component, typename ... Args>
    Component& AddComponent( Entity entity, Args&&... comp_args ); // replaces the old component if it already exists

    template<typename Component>
    Component* GetComponent( Entity entity ); // returns nullptr if the component of that type doesn't exist
    
    template<typename Component>
    const Component* GetComponent( Entity entity ) const;

    uint64_t GetEntityCount() const;

private:

    Entity2Components m_entities;

    friend struct details::ECSBTreeCallback<EntityContainer<Components...>>;

    template<typename Component>
    using BTreeCallback = details::ECSBTreeCallback<EntityContainer<Components...>>;

    template<typename Component>
    using ComponentBtree = btree_map<
        Entity,
        Component,
        CalcBTreeFactor<Component>( 0 ),
        std::allocator<btree_map_node<Entity, Component, CalcBTreeFactor<Component>( 0 )>>,
        BTreeCallback<Component>
    >;

    std::tuple<ComponentBtree<Components>...> m_components;
};

#include "EntityContainer.hpp"