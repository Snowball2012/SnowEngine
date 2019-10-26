#pragma once

#include "EntityContainer.h"

#include <typeindex>


template<typename ... Components>
EntityContainer<Components...>::EntityContainer()
    : m_components( BTreeCallback<Components>( *this )... )
    , m_components_virtual_storage( std::get<ComponentBtree<Components>>( m_components ) ... )
{
    m_components_virtual.reserve( sizeof...( Components ) );
    ( m_components_virtual.emplace( typeid( Components ).hash_code(), &std::get<IBtreeImpl<Components, ComponentBtree<Components>>>( m_components_virtual_storage ) ) , ... );
}


template<typename ... Components>
typename EntityContainer<Components...>::Entity EntityContainer<Components...>::CreateEntity()
{
    return m_entities.emplace();
}


template<typename ... Components>
void EntityContainer<Components...>::DestroyEntity( Entity entity )
{
    assert( entity != Entity::nullid );
    assert( m_entities.has( entity ) );

    for ( const auto& [component_id, cursor] : m_entities[entity] )
    {
        assert( m_components_virtual[component_id] );

        CursorPlaceholder ph_cursor;
        memcpy( &ph_cursor, &cursor, sizeof( cursor ) );
        m_components_virtual[component_id]->erase( ph_cursor );
    }

    m_entities.erase( entity );
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
template<typename Component>
void EntityContainer<Components...>::RemoveComponent( Entity entity )
{
    assert( entity != Entity::nullid );
    assert( m_entities.has( entity ) );

    auto& btree = std::get<ComponentBtree<Component>>( m_components );
    auto i = btree.find( entity );
    if ( i.valid() )
        btree.erase( i );

    m_entities[entity].erase( typeid( Component ).hash_code() );
}


template<typename ... Components>
uint64_t EntityContainer<Components...>::GetEntityCount() const
{
    return m_entities.size();
}


// Views
template<typename ...Components>
template<typename ...ViewComponents>
typename EntityContainer<Components...>::View<ViewComponents...>
    EntityContainer<Components...>::CreateView()
{
    View<ViewComponents...> retval( m_components );
    return retval;
}


template<typename ...Components>
template<typename ...ViewComponents>
typename EntityContainer<Components...>::View<const ViewComponents...>
    EntityContainer<Components...>::CreateView() const
{
    View<const ViewComponents...> retval( const_cast<std::remove_const_t<decltype( m_components )>&>( m_components ) );
    return retval;
}


template<typename ...Components>
template<typename ...ViewComponents>
typename EntityContainer<Components...>::View<ViewComponents...>::Iterator
    EntityContainer<Components...>::View<ViewComponents...>::begin()
{
    using ViewBtreeTuple = std::tuple<ComponentBtree<ViewComponents>&...>;
    ViewBtreeTuple view_btrees( std::get<ComponentBtree<ViewComponents>>( m_components )... );
    auto make_cursor_tuples = []( auto& ...tuple_btree )
    {
        return std::make_tuple( tuple_btree.begin() ... );
    }; // this awful piece of code was made entirely to satisfy msvc, sorry about that

    Iterator retval( std::apply( make_cursor_tuples, view_btrees ), m_components );
    retval.MakeCursorsEqual();

    const bool everything_is_equal = ( ( std::get<0>( retval.m_cursors ).key() == std::get<typename ComponentBtree<ViewComponents>::cursor_t>( retval.m_cursors ).key() ) && ... );
    if ( ! everything_is_equal )
        ++retval;

    return retval;
}


template<typename ...Components>
template<typename ...ViewComponents>
typename EntityContainer<Components...>::View<ViewComponents...>::Iterator
    EntityContainer<Components...>::View<ViewComponents...>::end()
{
    using ViewBtreeTuple = std::tuple<ComponentBtree<ViewComponents>&...>;
    ViewBtreeTuple view_btrees( std::get<ComponentBtree<ViewComponents>>( m_components )... );
    auto make_cursor_tuples = []( auto& ...tuple_btree )
    {
        return std::make_tuple( tuple_btree.end() ... );
    }; // this awful piece of code was made entirely to satisfy msvc, sorry about that

    return Iterator( std::apply( make_cursor_tuples, view_btrees ), m_components );
}


template<typename ...Components>
template<typename ...ViewComponents>
std::tuple<typename EntityContainer<Components...>::Entity, ViewComponents&...>
    EntityContainer<Components...>::View<ViewComponents...>::Iterator::operator*()
{
    assert( ( ( std::get<0>( m_cursors ).key() == std::get<typename ComponentBtree<ViewComponents>::cursor_t>( m_cursors ).key() ) && ... ) );
    return std::tie( std::get<0>( m_cursors ).key(), std::get<typename ComponentBtree<ViewComponents>::cursor_t>( m_cursors ).value()... );
}


template<typename ...Components>
template<typename ...ViewComponents>
typename EntityContainer<Components...>::View<ViewComponents...>::Iterator&
    EntityContainer<Components...>::View<ViewComponents...>::Iterator::operator++()
{
    auto& first_cursor = std::get<0>( m_cursors );
    using FirstBtreeT = ComponentBtree<std::decay_t<decltype(first_cursor.value())>>;
    auto& first_btree = std::get<FirstBtreeT>(m_components);
    first_cursor = first_btree.get_next( first_cursor );

    MakeCursorsEqual();

    return *this;
}


template<typename ...Components>
template<typename ...ViewComponents>
void EntityContainer<Components...>::View<ViewComponents...>::Iterator::MakeCursorsEqual()
{
    auto& first_cursor = std::get<0>( m_cursors );
    using FirstBtreeT = ComponentBtree<std::decay_t<decltype(first_cursor.value())>>;
    auto& first_btree = std::get<FirstBtreeT>(m_components);

    auto everything_is_equal = [&]() -> bool
    {
        return 
            ( ( std::get<0>( m_cursors ).valid() == std::get<typename ComponentBtree<ViewComponents>::cursor_t>( m_cursors ).valid()
                && ( std::get<0>( m_cursors ).valid()
                    ? ( std::get<0>( m_cursors ).key() == std::get<typename ComponentBtree<ViewComponents>::cursor_t>( m_cursors ).key() )
                    : true ) ) && ... );
    };

    Entity cur_entity;
    if ( first_cursor == first_btree.end() )
        cur_entity = Entity::nullid;
    else
        cur_entity = std::get<0>( m_cursors ).key();

    auto advance_cursor = [&]( auto& btree, auto& cursor, Entity& max_entity ) -> void
    {
        while ( cursor != btree.end() && cursor.key() < max_entity )
            cursor = btree.get_next( cursor );

        Entity cur_entity = Entity::nullid;
        if ( cursor != btree.end() )
            cur_entity = cursor.key();

        max_entity = cur_entity;
    };

    auto advance_all_cursors = [&]( auto&& ... cursors ) -> void
    {
        ( advance_cursor( std::get<ComponentBtree<std::decay_t<decltype( cursors.value() )>>>( m_components ), cursors, cur_entity ), ... );
    };

    while ( ! everything_is_equal() )
    {
        Entity prev_entity = cur_entity;
        std::apply( advance_all_cursors, m_cursors );
    }
}


template<typename ...Components>
template<typename ...ViewComponents>
bool EntityContainer<Components...>::View<ViewComponents...>::Iterator::operator!=( const Iterator& rhs ) const
{
    return std::get<0>( m_cursors ) != std::get<0>( rhs.m_cursors );
}


// callback implementation. messy
template<typename EC>
template<typename E, typename T>
void details::ECSBTreeCallback<EC>::operator()(
    const E& entity, const T& new_cursor ) const
{
    memcpy( &container.m_entities[entity][typeid(T).hash_code()], &new_cursor, sizeof( new_cursor ) );
}