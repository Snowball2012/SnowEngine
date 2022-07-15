#pragma once

// contigious span of elements
template<typename T>
class span
{
public:
    span( T* begin, T* end ) noexcept : m_begin( begin ), m_end( end ) { }
    span( const span<T>& other_span ) noexcept = default;
    span() noexcept : m_begin( nullptr ), m_end( nullptr ) {}

    operator span<const T>() const noexcept { return span<const T>( m_begin, m_end ); }

    T* begin() const noexcept { return m_begin; }
    T* end() const noexcept { return m_end; }
    const T* cbegin() const noexcept { return m_begin; }
    const T* cend() const noexcept { return m_end; }

    size_t size() const noexcept { return size_t(m_end - m_begin); }

    T& operator[]( size_t idx ) const noexcept { assert( m_begin + idx < m_end ); return m_begin[idx]; }

	T& back() const noexcept { assert( m_begin < m_end ); return *( m_end - 1 ); }
private:
    T* m_begin;
    T* m_end;
};

template<typename T>
span<T> make_span( T* begin, T* end )
{
    return span<T>( begin, end );
}

template<typename T, size_t N>
span<T> make_span( std::array<T, N>& arr )
{
    return span<T>( arr.data(), arr.data() + N );
}

template<typename T, size_t N>
span<const T> make_span( const std::array<T, N>& arr )
{
    return span<const T>( arr.data(), arr.data() + N );
}

template<typename T, size_t N>
span<T> make_span( T (&arr)[N] )
{
    return span<T>( arr, arr + N );
}

template<typename T, size_t N>
span<const T> make_span( const T (&arr)[N] )
{
    return span<const T>( arr, arr + N );
}

template<typename T>
span<T> make_span( std::vector<T>& vec )
{
    return span<T>( vec.data(), vec.data() + vec.size() );
}

template<typename T>
span<const T> make_const_span( std::vector<T>& vec )
{
    return span<const T>( vec.data(), vec.data() + vec.size() );
}

template<typename T>
span<const T> make_span( const std::vector<T>& vec )
{
    return span<const T>( vec.data(), vec.data() + vec.size() );
}

template<typename T>
span<const T> make_span( const std::initializer_list<T>& ilist )
{
    return span<const T>( std::begin( ilist ), std::end( ilist ) );
}

template<typename T>
span<const T> make_span( const span<T>& other_span )
{
    return span<const T>( other_span );
}

template<typename T>
span<T> make_span( span<T>& other_span )
{
    return span<T>( other_span );
}

template<typename T>
span<T> make_single_elem_span( T& elem )
{
    return make_span( std::addressof(elem), std::addressof(elem) + 1 );
}
