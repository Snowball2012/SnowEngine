#pragma once

// contigious span of elements
template<typename T>
class span
{
public:
	span( T* begin, T* end ) noexcept : m_begin( begin ), m_end( end ) { }
	span( const span<T>& other_span ) noexcept = default;
	span() noexcept : m_begin( nullptr ), m_end( nullptr ) {}

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