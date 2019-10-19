#pragma once

#include "EntityContainer.h"

#include <typeindex>


template<typename ... Components>
EntityContainer<Components...>::EntityContainer()
    : m_components( BTreeCallback<Components>( *this )... )
{
}


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
template<typename Component>
Component* EntityContainer<Components...>::GetComponent( Entity entity )
{
    assert( entity != Entity::nullid );
    assert( m_entities.has( entity ) );

    auto i = std::get<ComponentBtree<Component>>( m_components ).find( entity );

    if ( i.valid() )
        return &i.value();

    return nullptr;
}


template<typename ... Components>
template<typename Component>
const Component* EntityContainer<Components...>::GetComponent( Entity entity ) const
{
    assert( entity != Entity::nullid );
    assert( m_entities.has( entity ) );

    auto i = std::get<ComponentBtree<Component>>( m_components ).find( entity );

    if ( i.valid() )
        return &i.value();

    return nullptr;
}


template<typename ... Components>
uint64_t EntityContainer<Components...>::GetEntityCount() const
{
    return m_entities.size();
}


// callback implementation. messy
template<typename EC>
template<typename E, typename T>
void details::ECSBTreeCallback<EC>::operator()(
    const E& entity, const T& new_cursor ) const
{
    memcpy( &container.m_entities[entity][typeid(T).hash_code()], &new_cursor, sizeof( new_cursor ) );
}