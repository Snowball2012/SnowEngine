#pragma once

#include <boost/container/flat_map.hpp>

#include "utils/btree.h"
#include "utils/packed_freelist.h"

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

    Entity CreateEntity();

    template<typename Component, typename ... Args>
    Component& AddComponent( Entity entity, Args&&... comp_args ); // replaces the old component if it already exists

    uint64_t GetEntityCount() const;

private:

    Entity2Components m_entities;

    template<typename Component>
    using ComponentBtree = btree_map<
        Entity,
        Component,
        CalcBTreeFactor<Component>( 0 )
    >;

    std::tuple<ComponentBtree<Components>...> m_components;
};

#include "EntityContainer.hpp"