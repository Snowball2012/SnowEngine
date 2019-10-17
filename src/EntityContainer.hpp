#pragma once

#include "EntityContainer.h"


template<typename ... Components>
typename EntityContainer<Components...>::Entity EntityContainer<Components...>::CreateEntity()
{
    return m_entities.emplace();
}


template<typename ... Components>
template<typename Component, typename ... Args>
Component& EntityContainer<Components...>::AddComponent( Entity entity, Args&&... comp_args )
{
    assert( entity != Entity::nullid );
    assert( m_entities.has( entity ) );

    auto new_cursor = std::get<ComponentBtree<Component>>( m_components ).emplace( entity, std::forward<Args>( comp_args )... );

    assert( new_cursor.node );

    CursorPlaceholder ph;
    memcpy( &ph, &new_cursor, sizeof( ph ) );
    m_entities[entity][typeid(Component).hash_code()] = ph;

    return new_cursor.node->values()[new_cursor.position];
}


template<typename ... Components>
uint64_t EntityContainer<Components...>::GetEntityCount() const
{
    return m_entities.size();
}
