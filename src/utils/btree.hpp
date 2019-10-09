#pragma once

#include "btree.h"

#include <assert.h>

#define btree_map_class btree_map<Key, T, F, Allocator, PointerChangeCallback>

#define btree_map_method_definition( rettype )                   \
 template<typename Key, typename T, uint32_t F,                  \
          typename Allocator, typename PointerChangeCallback>    \
 rettype btree_map_class

#define btree_map_method_definition_constr                       \
 template<typename Key, typename T, uint32_t F,                  \
          typename Allocator, typename PointerChangeCallback>    \
 btree_map_class


btree_map_method_definition_constr::~btree_map()
{
    assert( m_root );
    destroy_node_recursive( m_root );
}


btree_map_method_definition_constr::btree_map()
{
    m_root = create_root();
}


btree_map_method_definition_constr::btree_map( const callback_t& callback )
    : m_notify_ptr_changed( callback )
{
    m_root = create_root();
}


btree_map_method_definition_constr::btree_map( allocator_t allocator )
    : m_allocator( std::move( allocator ) )
{
    m_root = create_root();
}


template<typename Key, typename T, uint32_t F,                  
         typename Allocator, typename PointerChangeCallback>             
template<typename... Args>
typename btree_map_class::cursor_t btree_map_class::emplace( const Key& key, Args&&... args )
{
    cursor_t new_elem_location = find_place_for_insertion( key );
    assert( new_elem_location.node->is_leaf() );

    auto& node = *new_elem_location.node;
    uint32_t pos = new_elem_location.position;

    auto* new_key = &node.keys()[pos];
    auto* new_value = &node.values()[pos];
    if ( *new_key != key )
    {
        make_space_for_new_elem( node, pos );
        new( new_key ) Key( key );
        new( new_value ) T( std::forward<Args>( args )... );
    }
    else
    {
        // replace old value with a new one
        *new_key = key;
        *new_value = T( std::forward<Args>( args )... );
    }

    m_size++;

    return cursor_t{ &node, pos };
}


btree_map_method_definition( typename btree_map_class::cursor_t )::insert( const Key& key, T elem )
{
    cursor_t new_elem_location = find_place_for_insertion( key );
    assert( new_elem_location.node->is_leaf() );

    auto& node = *new_elem_location.node;
    uint32_t pos = new_elem_location.position;

    auto* new_key = &node.keys()[pos];
    auto* new_value = &node.values()[pos];
    if ( *new_key != key || pos == node.num_elems )
    {
        make_space_for_new_elem( node, pos );
        new( new_key ) Key( key );
        new( new_value ) T( std::move( elem ) );
    }
    else
    {
        // replace old value with a new one
        *new_key = key;
        *new_value = std::move( elem );
    }

    m_size++;

    return cursor_t{ &node, pos };
}


btree_map_method_definition( typename btree_map_class::cursor_t )::find( const Key& key ) const
{
    return find_elem( key );
}


btree_map_method_definition( typename btree_map_class::cursor_t )::get_next( const cursor_t& pos ) const
{
    assert( pos.node );
    assert( pos.position < pos.node->num_elems );

    auto& node = *pos.node;

    if ( node.is_leaf() )
    {
        if ( pos.position + 1 < node.num_elems )
                return cursor_t{ &node, pos.position + 1 };

        cursor_t parent_cursor = node.parent;
        while ( parent_cursor.node )
        {
            if ( parent_cursor.node->num_elems > parent_cursor.position )
                return parent_cursor;
            parent_cursor = parent_cursor.node->parent;
        }

        parent_cursor.position = 0;
        return parent_cursor;
    }

    cursor_t retval{ node.children[pos.position + 1], 0 };
    while ( ! retval.node->is_leaf() )
        retval.node = retval.node->children[0];

    return retval;    
}


btree_map_method_definition( typename btree_map_class::cursor_t )::begin() const
{
    node_t* node = m_root;
    while ( ! node->is_leaf() )
        node = node->children[0];

    return cursor_t{ node, 0 };
}


btree_map_method_definition( typename btree_map_class::cursor_t )::end() const
{
    return cursor_t{ nullptr, 0 };
}


btree_map_method_definition( void )::clear()
{
    assert( m_root );
    destroy_node_recursive( m_root );

    m_size = 0;
    m_root = create_root();
}


btree_map_method_definition( void )::destroy_node_recursive( node_t* node ) noexcept
{
    assert( node );

    if ( ! node->is_leaf() )
        for ( uint32_t i = 0; i <= node->num_elems; ++i )
        {
            assert( node->children[i] );
            destroy_node_recursive( node->children[i] );
        }
    
    for ( uint32_t i = 0; i < node->num_elems; ++i )
    {
        node->values()[i].~T();
        node->keys()[i].~Key();
    }

    m_allocator.deallocate( node, 1 );
}


btree_map_method_definition( typename btree_map_class::node_t* )::create_empty_node( const cursor_t& cursor )
{
    node_t* new_node = m_allocator.allocate( 1 );
    assert( new_node );

    new_node->num_elems = 0;
    new_node->max_key = nullptr;
    new_node->parent = cursor;
    new_node->children[0] = nullptr;

    return new_node;
}


btree_map_method_definition( typename btree_map_class::node_t* )::create_root()
{
    cursor_t root_cursor
    {
        nullptr,
        0
    };
    return create_empty_node( root_cursor );
}


btree_map_method_definition( typename btree_map_class::cursor_t )::find_elem( const Key& key ) const
{
    assert( m_root );

    cursor_t retval = cursor_t{ nullptr, 0 };
    node_t* cur_node = m_root;
    while ( retval.node == nullptr )
    {
        uint32_t position = 0;
        for ( ; position < cur_node->num_elems; ++position )
        {
            if ( key == cur_node->keys()[position] )
            {
                retval.node = cur_node;
                retval.position = position;
                return retval;
            }
            if ( key < cur_node->keys()[position] )
                break;
        }

        if ( cur_node->is_leaf() )
            return retval;

        cur_node = cur_node->children[position];
    }

    return retval;
}


btree_map_method_definition( typename btree_map_class::cursor_t )::find_place_for_insertion( const Key& key )
{
    assert( m_root );

    if ( m_root->is_filled() )
    {
        node_t* new_root = create_root();
        m_root->parent = cursor_t{ new_root, 0 };
        new_root->children[0] = m_root;
        m_root = new_root;
    }

    cursor_t retval = cursor_t{ nullptr, 0 };
    node_t* cur_node = m_root;
    while ( retval.node == nullptr )
    {
        if ( cur_node->is_filled() )
        {
            if ( key == cur_node->keys()[MiddleIdx] )
            {
                retval.node = cur_node;
                retval.position = MiddleIdx;
                return retval;
            }
            node_t* right_node = split_node( cur_node );
            if ( key > cur_node->keys()[MiddleIdx] )
                cur_node = right_node;
        }

        uint32_t position = 0;
        for ( ; position < cur_node->num_elems; ++position )
        {
            if ( key == cur_node->keys()[position] )
            {
                retval.node = cur_node;
                retval.position = position;
                return retval;
            }
            if ( key < cur_node->keys()[position] )
                break;
        }

        if ( cur_node->is_leaf() )
        {
            retval.node = cur_node;
            retval.position = position;

            return retval;
        }
        else
        {
            cur_node = cur_node->children[position];
        }
    }

    return retval;
}


btree_map_method_definition( void )::make_space_for_new_elem( node_t& node, uint32_t pos )
{
    assert( node.num_elems < 2*F - 1 );
    assert( pos <= node.num_elems );
    
    node.num_elems++;

    if ( pos == node.num_elems - 1 )
        return;

    const auto& last_key = node.keys()[node.num_elems-1];
    const auto& last_val = node.values()[node.num_elems-1];

    new( &node.keys()[node.num_elems] ) Key( std::move( last_key ) );
    for ( uint32_t i = node.num_elems - 1; i > pos; --i )
        node.keys()[i] = std::move( node.keys()[i - 1] );
    node.keys()[pos].~Key();
    
    new( &node.values()[node.num_elems] ) T( std::move( last_val ) );
    for ( uint32_t i = node.num_elems - 1; i > pos; --i )
        node.values()[i] = std::move( node.values()[i - 1] );
    node.values()[pos].~T();

    for ( uint32_t i = node.num_elems - 1; i > pos; --i )
        m_notify_ptr_changed( node.keys()[i], cursor_t{ &node, i } );

}


btree_map_method_definition( typename btree_map_class::node_t* )::split_node( node_t* node )
{
    assert( node && node->parent.node );
    assert( ! node->parent.node->is_filled() );
    assert( node->is_filled() );

    auto& parent = node->parent;

    make_space_for_new_elem( *parent.node, parent.position );
    for ( uint32_t i = parent.node->num_elems - 1; i > parent.position; i-- )
    {
        parent.node->children[i] = parent.node->children[i-1];
        parent.node->children[i]->parent.position++;
    }

    new( &parent.node->keys()[parent.position] ) Key( std::move( node->keys()[MiddleIdx] ) );
    new( &parent.node->values()[parent.position] ) T( std::move( node->values()[MiddleIdx] ) );
    node->keys()[MiddleIdx].~Key();
    node->values()[MiddleIdx].~T();
    m_notify_ptr_changed( parent.node->keys()[parent.position], cursor_t{ parent.node, parent.position } );

    node_t* right_node = create_empty_node( cursor_t{ parent.node, parent.position + 1 } );
    parent.node->children[parent.position + 1] = right_node;
    for ( uint32_t i = 1; i < F; ++i )
    {
        new( &right_node->keys()[i - 1] ) Key( std::move( node->keys()[MiddleIdx + i] ) );
        node->keys()[MiddleIdx + i].~Key();
    }
    for ( uint32_t i = 1; i < F; ++i )
    {
        new( &right_node->values()[i - 1] ) Key( std::move( node->values()[MiddleIdx + i] ) );
        node->values()[MiddleIdx + i].~T();
    }
    if ( ! node->is_leaf() )
        for ( uint32_t i = 1; i <= F; ++i )
        {
            right_node->children[i - 1] = node->children[MiddleIdx + i];
            right_node->children[i - 1]->parent = cursor_t{ right_node, i - 1 };
        }

    node->num_elems = F - 1;
    right_node->num_elems = F - 1;

    for ( uint32_t i = 0; i < MiddleIdx; ++i )
        m_notify_ptr_changed( right_node->keys()[i], cursor_t{ right_node, i } );

    return right_node;
}


#undef btree_map_method_definition_const
#undef btree_map_method_definition
#undef btree_map_class