#pragma once

#define NOMINMAX

#include <vma/vk_mem_alloc.h>

#include <array>
#include <algorithm>
#include <iostream>
#include <vector>
#include <optional>
#include <span>
#include <set>
#include <queue>

#include <boost/container/small_vector.hpp>

#include <windows.h>

#include <utils/packed_freelist.h>

// double extension trick to stringify __LINE__ and other things
#define S(x) #x
#define S_(x) S(x)

#ifdef NDEBUG
#define INSERT_DBGBREAK
#else
#define INSERT_DBGBREAK DebugBreak();
#endif

#define VERIFY_EQUALS(x, ExpectedValue) \
if ((x) != (ExpectedValue)) \
{ \
	INSERT_DBGBREAK \
	throw std::runtime_error("Failed call to " #x " at " __FILE__ ":" S_(__LINE__) ", expected " #ExpectedValue); \
}

#define VERIFY_NOT_EQUAL(x, FailValue) \
if ((x) == (FailValue)) \
{ \
	INSERT_DBGBREAK \
	throw std::runtime_error("Failed call to " #x " at " __FILE__ ":" S_(__LINE__) ", expected not " #FailValue); \
}

#define VK_VERIFY(x) VERIFY_EQUALS(x, VK_SUCCESS)

#define NOTIMPL INSERT_DBGBREAK \
	throw std::runtime_error(__FUNCTION__ " is not implemented!");

#include <wrl.h>

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

#ifdef NDEBUG
#define SE_ENSURE( expr ) \
		(!!(expr))	
#else
#define SE_ENSURE( expr ) \
		((!!(expr)) || \
		(_wassert(_CRT_WIDE(#expr), _CRT_WIDE(__FILE__), (unsigned)(__LINE__)), 0))
#endif

#define SE_ENSURE_HRES( expr )\
		SE_ENSURE( SUCCEEDED(( expr )) )

#include <dxcapi.h>

#ifndef IMPLEMENT_SCOPED_ENUM_FLAGS
#define IMPLEMENT_SCOPED_ENUM_FLAGS(T) \
T operator|(T lhs, T rhs) \
{ \
	return T( \
		std::underlying_type<T>::type(lhs) \
		| std::underlying_type<T>::type(rhs)); \
} \
T operator&(T lhs, T rhs) \
{ \
	return T( \
		std::underlying_type<T>::type(lhs) \
		& std::underlying_type<T>::type(rhs)); \
}
#endif