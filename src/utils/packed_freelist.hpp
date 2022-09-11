#pragma once

#include "packed_freelist.h"

#include <cassert>


template<typename T, template <typename...> typename base_container>
const typename packed_freelist<T, base_container>::id packed_freelist<T, base_container>::id::nullid{ std::numeric_limits<uint32_t>::max(), 0 };


template<typename T, template <typename...> typename base_container>
bool packed_freelist<T, base_container>::has( id elem_id ) const noexcept
{
    if ( elem_id.idx < m_freelist.size() )
        return m_freelist[elem_id.idx].slot_cnt == elem_id.inner_id;
    else
        return false;
}


template<typename T, template <typename...> typename base_container>
typename packed_freelist<T, base_container>::id packed_freelist<T, base_container>::insert( T elem ) noexcept
{
    uint32_t packed_idx = uint32_t( m_packed_data.size() );
    m_packed_data.push_back( std::move( elem ) );

    id new_id = insert_elem_to_freelist( packed_idx );
    m_backlinks.push_back( new_id.idx );
    return new_id;
}


template<typename T, template <typename...> typename base_container>
template<typename ... Args>
typename packed_freelist<T, base_container>::id packed_freelist<T, base_container>::emplace( Args&& ... args ) noexcept
{
    uint32_t packed_idx = uint32_t( m_packed_data.size() );
    m_packed_data.emplace_back( std::forward<Args>( args )... );

    id new_id = insert_elem_to_freelist( packed_idx );
    m_backlinks.push_back( new_id.idx );
    return new_id;
}


template<typename T, template <typename...> typename base_container>
typename packed_freelist<T, base_container>::id packed_freelist<T, base_container>::insert_elem_to_freelist( uint32_t packed_idx ) noexcept
{
    id new_id;
    if ( m_free_head == FREE_END )
    {
        new_id.idx = uint32_t( m_freelist.size() );
        new_id.inner_id = 0;
        m_freelist.emplace_back( packed_idx );
    }
    else
    {
        freelist_elem& new_elem = m_freelist[m_free_head];
        new_id.idx = uint32_t( m_free_head );
        new_id.inner_id = new_elem.slot_cnt;
        m_free_head = new_elem.next_free;
        new_elem.packed_idx = packed_idx;
    }

    return new_id;
}


template<typename T, template <typename...> typename base_container>
T* packed_freelist<T, base_container>::try_get( id elem_id ) noexcept
{
    if ( has( elem_id ) )
        return &m_packed_data[m_freelist[elem_id.idx].packed_idx];
    return nullptr;
}


template<typename T, template <typename...> typename base_container>
const T* packed_freelist<T, base_container>::try_get( id elem_id ) const noexcept
{
    if ( has( elem_id ) )
        return &m_packed_data[m_freelist[elem_id.idx].packed_idx];
    return nullptr;
}


template<typename T, template <typename...> typename base_container>
T& packed_freelist<T, base_container>::get( id elem_id ) noexcept
{
    assert( has( elem_id ) );
    return m_packed_data[m_freelist[elem_id.idx].packed_idx];
}


template<typename T, template <typename...> typename base_container>
const T& packed_freelist<T, base_container>::get( id elem_id ) const noexcept
{
    assert( has( elem_id ) );
    return m_packed_data[m_freelist[elem_id.idx].packed_idx];
}


template<typename T, template <typename...> typename base_container>
const T& packed_freelist<T, base_container>::operator[]( id elem_id ) const noexcept
{
    return get( elem_id );
}


template<typename T, template <typename...> typename base_container>
T& packed_freelist<T, base_container>::operator[]( id elem_id ) noexcept
{
    return get( elem_id );
}


template<typename T, template <typename...> typename base_container>
void packed_freelist<T, base_container>::erase( id elem_id ) noexcept
{
    if ( ! has( elem_id ) )
        return;

    auto& freelist_elem = m_freelist[elem_id.idx];
    freelist_elem.slot_cnt++;

    using std::swap; // include to adl
    swap( m_packed_data.back(), m_packed_data[freelist_elem.packed_idx] );
    m_packed_data.pop_back();

    m_freelist[m_backlinks.back()].packed_idx = freelist_elem.packed_idx;
    swap( m_backlinks.back(), m_backlinks[freelist_elem.packed_idx]);
    m_backlinks.pop_back();


    freelist_elem.next_free = uint32_t( m_free_head );
    m_free_head = elem_id.idx;
}


template<typename T, template <typename...> typename base_container>
void packed_freelist<T, base_container>::clear( ) noexcept
{
    m_packed_data.clear();
    m_backlinks.clear();

    m_free_head = m_freelist.empty() ? FREE_END : 0;

    if (m_freelist.empty())
        return;

    m_freelist.back().next_free = FREE_END;
    m_freelist.back().slot_cnt++;
    for ( uint32_t i = 0, end = uint32_t( m_freelist.size() ) - 1; i < end; ++i )
    {
        m_freelist[i].next_free = i + 1;
        m_freelist[i].slot_cnt++;
    }
}


template<typename T, template <typename...> typename base_container>
void packed_freelist<T, base_container>::destroy() noexcept
{
    m_packed_data.clear();
    m_backlinks.clear();
    m_freelist.clear();
    m_free_head = FREE_END;
}


template<typename T, template <typename...> typename base_container>
size_t packed_freelist<T, base_container>::size() const noexcept
{
    return m_packed_data.size();
}


template<typename T, template <typename...> typename base_container>
size_t packed_freelist<T, base_container>::capacity() const noexcept
{
    return m_packed_data.capacity();
}


template<typename T, template <typename...> typename base_container>
void packed_freelist<T, base_container>::reserve( uint32_t nelems ) noexcept
{
    m_packed_data.reserve( nelems );
    m_freelist.reserve( nelems );
    m_backlinks.reserve( nelems );
}


template<typename T, template <typename...> typename base_container>
void packed_freelist<T, base_container>::shrink_to_fit( ) noexcept
{
    m_packed_data.shrink_to_fit();
    m_freelist.shrink_to_fit();
    m_backlinks.shrink_to_fit();
}


template<typename T, template <typename...> typename base_container>
typename packed_freelist<T, base_container>::iterator packed_freelist<T, base_container>::begin() noexcept
{
    return m_packed_data.begin();
}


template<typename T, template <typename...> typename base_container>
typename packed_freelist<T, base_container>::const_iterator packed_freelist<T, base_container>::begin() const noexcept
{
    return m_packed_data.begin();
}


template<typename T, template <typename...> typename base_container>
typename packed_freelist<T, base_container>::iterator packed_freelist<T, base_container>::end() noexcept
{
    return m_packed_data.end();
}


template<typename T, template <typename...> typename base_container>
typename packed_freelist<T, base_container>::const_iterator packed_freelist<T, base_container>::end() const noexcept
{
    return m_packed_data.end();
}


template<typename T, template <typename...> typename base_container>
typename packed_freelist<T, base_container>::const_iterator packed_freelist<T, base_container>::cbegin() const noexcept
{
    return m_packed_data.cbegin();
}


template<typename T, template <typename...> typename base_container>
typename packed_freelist<T, base_container>::const_iterator packed_freelist<T, base_container>::cend() const noexcept
{
    return m_packed_data.cend();
}


template<typename T, template <typename...> typename base_container>
span<T> packed_freelist<T, base_container>::get_elems() noexcept
{
    return make_span( m_packed_data );
}


template<typename T, template <typename...> typename base_container>
span<const T> packed_freelist<T, base_container>::get_elems() const noexcept
{
    return make_span( m_packed_data );
}
