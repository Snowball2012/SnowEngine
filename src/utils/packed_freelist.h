#pragma once

#include <vector>
#include <optional>

// packed vector-based freelist, provides persistent ids for all its elements.
// O(1) access by id (2 lookups in base_container)
// all const methods are thread-safe 
// no thread safety on any non-const method
// 
// elements must be movable
// base_container must be random access, std::vector-like container
// max 2^32 elems simultaneously in freelist

// contigious span of elements
template<typename T>
class span
{
public:
	span( T* begin, T* end ) noexcept : m_begin( begin ), m_end( end ) { }
	span( const span<T>& other_span ) noexcept = default;

	operator span<const T>() noexcept { return span<const T>( m_begin, m_end ); }

	template<typename U = T>
	std::enable_if_t<!std::is_const_v<U>, U*> begin() noexcept { return m_begin; }
	template<typename U = T>
	std::enable_if_t<!std::is_const_v<U>, U*> end() noexcept { return m_end; }
	const T* begin() const noexcept { return m_begin; }
	const T* end() const noexcept { return m_end; }
	const T* cbegin() const noexcept { return m_begin; }
	const T* cend() const noexcept { return m_end; }

	size_t size() const noexcept { return m_end - m_begin; }

	T& operator[]( size_t idx ) noexcept { assert( m_begin + idx < m_end ); return m_begin[idx]; }
	const T& operator[]( size_t idx ) const noexcept { assert( m_begin + idx < m_end ); return m_begin[idx]; }
private:
	T* m_begin;
	T* m_end;
};

template<typename T>
span<T> make_span( T* begin, T* end )
{
	return span<T>( begin, end );
}

template<typename T, template <typename ...> typename base_container = std::vector>
class packed_freelist
{
public:
	struct alignas( 8 ) id
	{
		uint32_t idx;
		uint32_t inner_id;
		bool operator==( const id& rhs ) const noexcept { return this->idx == rhs.idx && this->inner_id == rhs.inner_id; }

		static const id nullid;
	};


	bool has( id elem_id ) const noexcept;

	id insert( T elem ) noexcept;
	template<typename ... Args>
	id emplace( Args&& ... args ) noexcept;

	// returns nullptr if elem does not exist
	T* try_get( id elem_id ) noexcept;
	const T* try_get( id elem_id ) const noexcept;

	// ub if element with elem_id has been deleted
	T& get( id elem_id ) noexcept;
	const T& get( id elem_id ) const noexcept;

	// ub if element with elem_id has been deleted
	T& operator[]( id elem_id ) noexcept;
	const T& operator[]( id elem_id ) const noexcept;

	// does nothing if element does not exist
	void erase( id elem_id ) noexcept;
	void clear() noexcept;

	// semanticaly the same as clear(), but also destroys slot counters for the nodes, so it will invalidate all previously given ids
	// use it over clear only if you want to reclaim memory afterwards with shink_to_fit() call
	void destroy() noexcept;

	size_t size() const noexcept;
	size_t capacity() const noexcept;

	void reserve( uint32_t nelems ) noexcept;
	void shrink_to_fit() noexcept;

	using iterator = typename std::vector<T>::iterator;
	using const_iterator = typename std::vector<T>::const_iterator;

	// these traversal iterators may be invalidated after a call to any non-const method except get(), try_get() and operator[]
	iterator begin() noexcept;
	iterator end() noexcept;

	const_iterator begin() const noexcept;
	const_iterator end() const noexcept;

	const_iterator cbegin() const noexcept;
	const_iterator cend() const noexcept;

	span<T> get_elems() noexcept;
	span<const T> get_elems() const noexcept;

private:
	struct freelist_elem
	{
		union
		{
			uint32_t packed_idx;
			uint32_t next_free;
		};
		uint32_t slot_cnt;

		freelist_elem( uint32_t offset ) : packed_idx( offset ), slot_cnt( 0 ) {}
		freelist_elem() = default;
	};

	static constexpr uint32_t FREE_END = std::numeric_limits<uint32_t>::max();

	id insert_elem_to_freelist( uint32_t packed_idx ) noexcept;

	base_container<T> m_packed_data;
	base_container<freelist_elem> m_freelist;
	uint32_t m_free_head = FREE_END;
};

#include "packed_freelist.hpp"
