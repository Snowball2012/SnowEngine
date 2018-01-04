#pragma once

#include <cstddef>

template<class T, std::size_t N, std::size_t Align = alignof(std::max_align_t)>
class small_allocator
{
public:
	using value_type = T;
	static constexpr auto size = N;

	T* allocate( std::size_t n )
	{

	}



private:
	alignas( Align ) char m_buf[N];
	size_t m_buf_used = 0;
};