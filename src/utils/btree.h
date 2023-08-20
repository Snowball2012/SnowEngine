#pragma once

#include <memory>
#include <utility>

template<typename Key, typename T, uint32_t F>
struct btree_map_node;

template<typename Key, typename T, uint32_t F>
struct btree_map_cursor
{
    using value_t = T;

    btree_map_node<Key, T, F>* node;
    uint32_t position;

    bool valid() const { return node != nullptr; }
    Key& key() { return node->keys()[position]; }
    T& value() { return node->values()[position]; }

    const Key& key() const { return node->keys()[position]; }
    const T& value() const { return node->values()[position]; }

    bool operator!= ( const btree_map_cursor& rhs ) const { return this->node != rhs.node || this->position != rhs.position; }
    bool operator== ( const btree_map_cursor& rhs ) const { return this->node == rhs.node && this->position == rhs.position; }
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

    static constexpr uint32_t MiddleIdx = F - 1;
public:

    using cursor_t = btree_map_cursor<Key, T, F>;

    ~btree_map();

    btree_map();
    btree_map( const callback_t& callback );
    btree_map( allocator_t allocator );
    btree_map( const callback_t& callback, allocator_t allocator );

	btree_map( const btree_map& ) = delete;
	btree_map& operator=( const btree_map& ) = delete;
	btree_map( btree_map&& ) = delete;
	btree_map& operator=( btree_map&& ) = delete;

    template<typename... Args>
    cursor_t emplace( const Key& key, Args&&... args );
    cursor_t insert( const Key& key, T elem );

    cursor_t erase( const cursor_t& pos );

    cursor_t find( const Key& key ) const;

    cursor_t get_next( const cursor_t& pos ) const;

    cursor_t begin() const;
    cursor_t end() const;

	template<typename Visitor>
	void for_each(Visitor&& visitor);

    void clear();

    size_t size() const noexcept { return m_size; }

private:
    node_t* m_root = nullptr;
    callback_t m_notify_ptr_changed;
    size_t m_size = 0;
    allocator_t m_allocator;

    // a node must not be null
    void destroy_node_recursive( node_t* node ) noexcept;

    node_t* create_empty_node( const cursor_t& cursor );
    node_t* create_root();

    cursor_t find_elem( const Key& key ) const;

    cursor_t find_place_for_insertion( const Key& key );

    void make_space_for_new_elem( node_t& node, uint32_t pos );

    // a node must have a non-null parent (be aware of the root node) and be full
    // returns the right node
    node_t* split_node( node_t* node );

    void remove_blank_val_at( node_t& node, uint32_t pos );

    void update_max( node_t* parent, uint32_t child_pos, const Key* new_max );
    cursor_t get_next_for_child( node_t* parent, uint32_t pos );

    // a node must have a parent and less than F - 1 elems
    void merge_with_neighbour( node_t* node );

    void notify_cursor_change( node_t& node, uint32_t pos );
    void notify_cursor_change( cursor_t new_cursor );
	
	template<typename Visitor>
	void for_each(node_t& node, Visitor&& visitor);
};

#include "btree.hpp"
