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

private:

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

     // reflection stuff to be able to remove components from entities by a component typeid
    struct IBtree
    {
        virtual void erase( Entity ent ) = 0;

        virtual void erase( CursorPlaceholder cursor ) = 0;
    };

    template<typename C, typename Btree>
    class IBtreeImpl : public IBtree
    {
    public:
        virtual void erase( Entity ent ) override
        {
            auto i = m_components.find( ent );
            if ( i != m_components.end() )
                m_components.erase( i );
        }

        virtual void erase( CursorPlaceholder cursor_ph ) override
        {
            typename Btree::cursor_t cursor;
            memcpy( &cursor, &cursor_ph, sizeof( cursor ) );
            assert( cursor != m_components.end() );
            m_components.erase( cursor );
        }


        IBtreeImpl( Btree& components ): m_components( components ) {}
    private:
        Btree& m_components;
    };

public:

    // Noncopyable, nonmovable
    EntityContainer();
    EntityContainer( const EntityContainer& ) = delete;
    EntityContainer( EntityContainer&& ) = delete;
    EntityContainer& operator=( const EntityContainer& ) = delete;
    EntityContainer& operator=( EntityContainer&& ) = delete;

    Entity CreateEntity();
    void DestroyEntity( Entity entity );

    template<typename Component, typename ... Args>
    Component& AddComponent( Entity entity, Args&&... comp_args ); // replaces the old component if it already exists

    template<typename Component>
    Component* GetComponent( Entity entity ); // returns nullptr if the component of that type doesn't exist
    
    template<typename Component>
    const Component* GetComponent( Entity entity ) const;

    template<typename Component>
    void RemoveComponent( Entity entity );

    uint64_t GetEntityCount() const;

    // todo: get number of components of each type

    // Views
    template<typename ...ViewComponents>
    class View
    {
    public:
        class Iterator
        {
        public:
            std::tuple<Entity, ViewComponents&...> operator*();
            Iterator& operator++();
            bool operator!=( const Iterator& rhs ) const;

        private:
            friend class View;
            Iterator( const std::tuple<typename ComponentBtree<ViewComponents>::cursor_t...>& cursors, std::tuple<ComponentBtree<Components>...>& components )
                : m_cursors( cursors ), m_components( components )
            {}

            void MakeCursorsEqual();

            std::tuple<typename ComponentBtree<ViewComponents>::cursor_t...> m_cursors;
            std::tuple<ComponentBtree<Components>...>& m_components;
        };

        Iterator begin();
        Iterator end();

    private:
        friend class EntityContainer;
        View( std::tuple<ComponentBtree<Components>...>& components )
            : m_components( components )
        {}

        std::tuple<ComponentBtree<Components>...>& m_components;
    };

    template<typename ...ViewComponents>
    View<ViewComponents...> CreateView();

    // todo: const views
    template<typename ...ViewComponents>
    View<const ViewComponents...> CreateView() const;


private:

    Entity2Components m_entities;   

    std::tuple<ComponentBtree<Components>...> m_components;

    std::tuple<IBtreeImpl<Components, ComponentBtree<Components>>...> m_components_virtual_storage;
    boost::container::flat_map<
        size_t /*component_typeid*/,
        IBtree* /*component_btree*/
    > m_components_virtual;
};

#include "EntityContainer.hpp"