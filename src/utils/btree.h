#pragma once

#include <memory>
#include <utility>

template<typename Key, typename T, uint32_t F>
struct btree_map_node;

template<typename Key, typename T, uint32_t F>
struct btree_map_cursor
{
    btree_map_node<Key, T, F>* node;
    uint32_t position;
};

template<typename Key, typename T, uint32_t F>
struct btree_map_node
{
    alignas(Key) char key_storage[sizeof(Key)*(2*F - 1)];
    btree_map_node<Key, T, F>* children[2*F];
    const Key* max_key; // including this node's children
    btree_map_cursor<Key, T, F> parent;
    uint32_t num_elems;
    alignas(T) char value_storage[sizeof(T)*(2*F - 1)];

    // simple accessors for convenience
    Key* keys() { return reinterpret_cast<Key*>( key_storage ); }
    const Key* keys() const { return reinterpret_cast<const Key*>( key_storage ); }

    T* values() { return reinterpret_cast<T*>( value_storage ); }
    const T* values() const { return reinterpret_cast<const T*>( value_storage ); }

    bool is_leaf() const { return !children[0]; }
    bool is_filled() const { return num_elems == 2*F - 1; }
};

template<typename Key, typename T, uint32_t F>
struct btree_map_default_callback
{
    void operator()( const Key& key, const btree_map_cursor<Key, T, F>& new_cursor ) const {}
};


template<typename Key, typename T, uint32_t F,
         typename Allocator = std::allocator<btree_map_node<Key, T, F>>,
         typename PointerChangeCallback = btree_map_default_callback<Key, T, F>>
class btree_map
{
    using node_t = btree_map_node<Key, T, F>;
    using allocator_t = Allocator;
    using callback_t = PointerChangeCallback;
public:

    using cursor_t = btree_map_cursor<Key, T, F>;

    ~btree_map();

    btree_map();
    btree_map( allocator_t allocator );

    template<typename... Args>
    cursor_t emplace( const Key& key, Args&&... args );
    cursor_t insert( const Key& key, T elem );

    void clear();

    size_t size() const noexcept { return m_size; }

private:
    allocator_t m_allocator;
    node_t* m_root = nullptr;
    callback_t m_notify_ptr_changed;
    size_t m_size = 0;

    // a node must not be null
    void destroy_node_recursive( node_t* node ) noexcept;

    node_t* create_empty_node( const cursor_t& cursor );
    node_t* create_root();

    template<bool InsertMode>
    cursor_t find_elem( const Key& key );

    void make_space_for_new_elem( node_t& node, uint32_t pos );

    // a node must have a non-null parent (be aware of the root node) and be full
    // returns the right node
    node_t* split_node( node_t* node );
};

#include "btree.hpp"
