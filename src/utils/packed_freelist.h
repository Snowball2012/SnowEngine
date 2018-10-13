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

struct alignas( 8 ) freelist_id
{
	uint32_t idx;
	uint32_t inner_id;
};

template<typename T, template <typename ...> typename base_container = std::vector>
class packed_freelist
{
public:
	bool has( freelist_id elem_id ) const noexcept;

	freelist_id insert( T elem ) noexcept;
	template<typename ... Args>
	freelist_id emplace( Args&& ... args ) noexcept;

	// returns nullptr if elem does not exist
	T* try_get( freelist_id elem_id ) noexcept;
	const T* try_get( freelist_id elem_id ) const noexcept;

	// ub if element with elem_id has been deleted
	T& get( freelist_id elem_id ) noexcept;
	const T& get( freelist_id elem_id ) const noexcept;

	// ub if element with elem_id has been deleted
	T& operator[]( freelist_id elem_id ) noexcept;
	const T& operator[]( freelist_id elem_id ) const noexcept;

	// does nothing if element does not exist
	void erase( freelist_id elem_id ) noexcept;
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

	freelist_id insert_elem_to_freelist( uint32_t packed_idx ) noexcept;

	base_container<T> m_packed_data;
	base_container<freelist_elem> m_freelist;
	int64_t m_free_head = -1;
};

#include "packed_freelist.hpp"
