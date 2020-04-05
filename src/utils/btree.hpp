#pragma once

#include "btree.h"

#include <assert.h>
#include <optional>

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


btree_map_method_definition_constr::btree_map( const callback_t& callback, allocator_t allocator )
    : m_notify_ptr_changed( callback ), m_allocator( std::move( allocator ) )
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
    if ( *new_key != key || pos == node.num_elems )
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

template<typename Key, typename T, uint32_t F,                  
         typename Allocator, typename PointerChangeCallback>             
template<typename Visitor>
void btree_map_class::for_each( Visitor&& visitor )
{
	assert( m_root );
	for_each( *m_root, visitor );
}

template<typename Key, typename T, uint32_t F,                  
         typename Allocator, typename PointerChangeCallback>             
template<typename Visitor>
void btree_map_class::for_each( node_t& node, Visitor&& visitor )
{
	for ( uint32_t i = 0; i < node.num_elems; ++i )
		visitor(node.keys()[i], node.values()[i]);

	if ( !node.is_leaf() )
	{
		for ( uint32_t i = 0; i < node.num_elems + 1; ++i )
		{
			assert( node.children[i] );
			for_each( *node.children[i], visitor );
		}
	}
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


btree_map_method_definition( typename btree_map_class::cursor_t )::erase( const cursor_t& cursor )
{
    assert( cursor.node && m_size > 0 );

    auto& node = *cursor.node;
    uint32_t pos = cursor.position;

    if ( node.is_leaf() )
    {
        m_size--;
        remove_blank_val_at( node, pos );
        if ( pos == node.num_elems && pos != 0 )
            node.max_key = &node.keys()[pos - 1];
        if ( node.num_elems >= ( F - 1 ) || ! node.parent.node )
        {            
            if ( cursor.position == node.num_elems )
            {
                if ( node.parent.node )
                {
                    update_max( node.parent.node, node.parent.position, &node.keys()[pos - 1] );
                    return get_next_for_child( node.parent.node, node.parent.position );
                }
                else
                {
                    return cursor_t{ nullptr, 0 };
                }
            }

            return cursor;
        }
        else
        {
            cursor_t next_cursor = node.num_elems > pos
                ? cursor
                : get_next_for_child( node.parent.node, node.parent.position );

            std::optional<Key> next_key;
            if ( next_cursor != end() )
                next_key = next_cursor.node->keys()[next_cursor.position];

            merge_with_neighbour( cursor.node );

            if ( next_key )
                return find_elem( next_key.value() );
            else
                return cursor_t{ nullptr, 0 };
        }
    }
    else
    {
        // find next element, replace this one with it and remove it in the subtree
        cursor_t next = get_next( cursor );
        assert( next.node && next.node->is_leaf() );
        node.keys()[pos] = std::move( next.node->keys()[next.position] );
        node.values()[pos] = std::move( next.node->values()[next.position] );

        notify_cursor_change( cursor );

        return erase( next );
    }
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
    if ( m_size == 0 )
        return end();

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

    const auto& last_key = node.keys()[node.num_elems-2];
    const auto& last_val = node.values()[node.num_elems-2];

    new( &node.keys()[node.num_elems - 1] ) Key( std::move( last_key ) );
    for ( uint32_t i = node.num_elems - 2; i > pos; --i )
        node.keys()[i] = std::move( node.keys()[i - 1] );
    node.keys()[pos].~Key();
    
    new( &node.values()[node.num_elems - 1] ) T( std::move( last_val ) );
    for ( uint32_t i = node.num_elems - 2; i > pos; --i )
        node.values()[i] = std::move( node.values()[i - 1] );
    node.values()[pos].~T();

    for ( uint32_t i = node.num_elems - 1; i > pos; --i )
        notify_cursor_change( node, i );
}


btree_map_method_definition( typename btree_map_class::node_t* )::split_node( node_t* node )
{
    assert( node && node->parent.node );
    assert( ! node->parent.node->is_filled() );
    assert( node->is_filled() );

    auto parent = node->parent;

    make_space_for_new_elem( *parent.node, parent.position );
    for ( uint32_t i = parent.node->num_elems; i > parent.position + 1; i-- )
    {
        parent.node->children[i] = parent.node->children[i-1];
        parent.node->children[i]->parent.position++;
    }

    new( &parent.node->keys()[parent.position] ) Key( std::move( node->keys()[MiddleIdx] ) );
    new( &parent.node->values()[parent.position] ) T( std::move( node->values()[MiddleIdx] ) );
    node->keys()[MiddleIdx].~Key();
    node->values()[MiddleIdx].~T();
    notify_cursor_change( parent );

    node_t* right_node = create_empty_node( cursor_t{ parent.node, parent.position + 1 } );
    parent.node->children[parent.position + 1] = right_node;

    for ( uint32_t i = 1; i < F; ++i )
    {
        new( &right_node->keys()[i - 1] ) Key( std::move( node->keys()[MiddleIdx + i] ) );
        node->keys()[MiddleIdx + i].~Key();
    }
    for ( uint32_t i = 1; i < F; ++i )
    {
        new( &right_node->values()[i - 1] ) T( std::move( node->values()[MiddleIdx + i] ) );
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
        notify_cursor_change( *right_node, i );

    return right_node;
}


btree_map_method_definition( void )::remove_blank_val_at( node_t& node, uint32_t pos )
{
    assert( node.num_elems > pos );
    
    for ( uint32_t i = pos + 1; i < node.num_elems; ++i )
        node.keys()[i - 1] = std::move( node.keys()[i] );
    
    for ( uint32_t i = pos + 1; i < node.num_elems; ++i )
        node.values()[i - 1] = std::move( node.values()[i] );

    if ( !node.is_leaf() )
        for ( uint32_t i = pos + 1; i < node.num_elems; ++i )
        {
            node.children[i] = node.children[i + 1];
            node.children[i]->parent.position = i;
        }

    node.num_elems--;

    node.keys()[node.num_elems].~Key();
    node.values()[node.num_elems].~T();

    for ( uint32_t i = pos; i < node.num_elems; ++i )
        notify_cursor_change( node, i );
}


btree_map_method_definition( void )::update_max( node_t* parent, uint32_t child_pos, const Key* new_max )
{
    assert( parent && ! parent->is_leaf() );

    while( parent )
    {
        if ( parent->num_elems != child_pos )
            return;
        
        parent->max_key = new_max;
        child_pos = parent->parent.position;
        parent = parent->parent.node;
    }
}

btree_map_method_definition( typename btree_map_class::cursor_t )::get_next_for_child( node_t* parent, uint32_t pos )
{
    while ( parent )
    {
        if ( pos < parent->num_elems )
            return cursor_t{ parent, pos };
        pos = parent->parent.position;
        parent = parent->parent.node;
    }

    return cursor_t{ nullptr, 0 };
}

btree_map_method_definition( void )::merge_with_neighbour( node_t* node )
{
    assert( node->parent.node && node->num_elems < F - 1 );

    node_t* parent = node->parent.node;
    while( parent )
    {
        // find the biggest neighbour
        node_t* neighbour = nullptr;
        uint32_t split_pos = node->parent.position;

        if ( split_pos < parent->num_elems )
            neighbour = parent->children[split_pos + 1];

        if ( split_pos > 0 )
            if ( ! neighbour || neighbour->num_elems == F - 1 )
            {
                neighbour = node;
                node = parent->children[--split_pos];
            }

        // neighbour is on the right, node is on the left
        if ( neighbour->num_elems > F - 1 || node->num_elems > F - 1 ) //-V522
        {
            if ( neighbour->num_elems == F - 2 )
            {
                // move 1 item from the left to the right
                new ( &neighbour->keys()[neighbour->num_elems] ) Key( std::move( neighbour->keys()[neighbour->num_elems - 1] ) );
                new ( &neighbour->values()[neighbour->num_elems] ) T( std::move( neighbour->values()[neighbour->num_elems - 1] ) );

                notify_cursor_change( *neighbour, neighbour->num_elems );

                for ( int i = int( neighbour->num_elems ) - 1; i > 0; --i )
                    neighbour->keys()[i] = std::move( neighbour->keys()[i - 1] );
                neighbour->keys()[0] = std::move( parent->keys()[split_pos] );

                for ( int i = int( neighbour->num_elems ) - 1; i > 0; --i )
                    neighbour->values()[i] = std::move( neighbour->values()[i - 1] );
                neighbour->values()[0] = std::move( parent->values()[split_pos] );

                for ( int i = 0; i < int( neighbour->num_elems ); ++i )
                    notify_cursor_change( *neighbour, i );

                parent->keys()[split_pos] = std::move( node->keys()[node->num_elems - 1] );
                parent->values()[split_pos] = std::move( node->values()[node->num_elems - 1] );

                notify_cursor_change( *parent, split_pos );

                node->keys()[node->num_elems - 1].~Key();
                node->values()[node->num_elems - 1].~T();

                if ( node->is_leaf() )
                {
                    //update max
                    node->max_key = &node->keys()[node->num_elems - 2];
                }
                else
                {
                    // update children
                    for ( int i = neighbour->num_elems + 1; i > 0; --i )
                    {
                        neighbour->children[i] = neighbour->children[i - 1];
                        neighbour->children[i]->parent.position = i;
                    }

                    neighbour->children[0] = node->children[node->num_elems];
                    neighbour->children[0]->parent.node = neighbour;
                    neighbour->children[0]->parent.position = 0;

                    node->max_key = node->children[node->num_elems - 1]->max_key;
                }
                node->num_elems--;
                neighbour->num_elems++;
            }
            else
            {
                // move 1 item from the right to the left
                new( &node->keys()[node->num_elems] ) Key( std::move( parent->keys()[split_pos] ) );
                new( &node->values()[node->num_elems] ) T( std::move( parent->values()[split_pos] ) );

                notify_cursor_change( *node, node->num_elems );

                parent->keys()[split_pos] = std::move( neighbour->keys()[0] );
                parent->values()[split_pos] = std::move( neighbour->values()[0] );

                notify_cursor_change( *parent, split_pos );

                for ( int i = 0; i < int( neighbour->num_elems ) - 1; ++i )
                    neighbour->keys()[i] = std::move( neighbour->keys()[i + 1] );
                neighbour->keys()[neighbour->num_elems - 1].~Key();

                for ( int i = 0; i < int( neighbour->num_elems ) - 1; ++i )
                    neighbour->values()[i] = std::move( neighbour->values()[i + 1] );
                neighbour->values()[neighbour->num_elems - 1].~T();
                
                for ( int i = 0; i < int( neighbour->num_elems ) - 1; ++i )
                    notify_cursor_change( *neighbour, i );

                if ( neighbour->is_leaf() )
                {
                    //update max
                    node->max_key = &node->keys()[node->num_elems];
                }
                else
                {
                    // update children
                    node->children[node->num_elems + 1] = neighbour->children[0];
                    node->children[node->num_elems + 1]->parent.node = node;
                    node->children[node->num_elems + 1]->parent.position = node->num_elems + 1;
                    for ( uint32_t i = 0; i < neighbour->num_elems; ++i )
                    {
                        neighbour->children[i] = neighbour->children[i + 1];
                        neighbour->children[i]->parent.position = i;
                    }

                    node->max_key = node->children[node->num_elems + 1]->max_key;
                }
                
                node->num_elems++;
                neighbour->num_elems--;
            }

            return;
        }
        else
        {
            // merge the nodes
            new( &node->keys()[node->num_elems] ) Key( std::move( parent->keys()[split_pos] ) );
            new( &node->values()[node->num_elems] ) T( std::move( parent->values()[split_pos] ) );
            notify_cursor_change( *node, node->num_elems );

            remove_blank_val_at( *parent, split_pos );
            
            for ( uint32_t i = 0; i < neighbour->num_elems; ++i )
            {
                new (&node->keys()[node->num_elems + 1 + i]) Key( std::move( neighbour->keys()[i] ) );
                neighbour->keys()[i].~Key();
            }
                
            for ( uint32_t i = 0; i < neighbour->num_elems; ++i )
            {
                new (&node->values()[node->num_elems + 1 + i]) T( std::move( neighbour->values()[i] ) );
                neighbour->values()[i].~T();
            }

            for ( uint32_t i = 0; i < neighbour->num_elems; ++i )
                notify_cursor_change( *node, node->num_elems + 1 + i );

            if ( ! neighbour->is_leaf() )
            {
                for ( uint32_t i = 0; i <= neighbour->num_elems; ++i )
                {
                    uint32_t dst_idx = node->num_elems + 1 + i;
                    node->children[dst_idx] = neighbour->children[i];
                    node->children[dst_idx]->parent = cursor_t{ node, dst_idx };
                }
            }

            node->max_key = neighbour->is_leaf() ? &node->keys()[2 * F - 3] : neighbour->max_key;
            update_max( parent, split_pos, node->max_key );
            node->num_elems = 2*F - 2;

            m_allocator.deallocate( neighbour, 1 );
            node = parent;
            if ( node->num_elems >= F - 1 )
                break;

            parent = node->parent.node;
        }
    }

    if ( node->num_elems == 0 )
    {
        assert( m_root == node );
        assert( ! node->is_leaf() );

        m_root = node->children[0];
        m_root->parent = cursor_t{ nullptr, 0 };
    }
}


btree_map_method_definition( void )::notify_cursor_change( node_t& node, uint32_t pos )
{
    m_notify_ptr_changed( node.keys()[pos], cursor_t{ &node, pos } );
}


btree_map_method_definition( void )::notify_cursor_change( cursor_t new_cursor )
{
    assert( new_cursor.valid() );
    notify_cursor_change( *new_cursor.node, new_cursor.position );
}

#undef btree_map_method_definition_constr
#undef btree_map_method_definition
#undef btree_map_class