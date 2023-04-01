#pragma once

#define NOMINMAX

#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#include <atomic>
#include <array>
#include <algorithm>
#include <iostream>
#include <mutex>
#include <vector>
#include <optional>
#include <span>
#include <set>
#include <queue>
#include <memory>
#include <chrono>
#include <codecvt>
#include <locale>

#include <boost/container/small_vector.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/scope_exit.hpp>

namespace bc = boost::container;

#include <windows.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

#define NOTIMPL do { INSERT_DBGBREAK \
	throw std::runtime_error(__FUNCTION__ " is not implemented!"); } while(0);

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

#ifndef IMPLEMENT_SCOPED_ENUM_FLAGS
#define IMPLEMENT_SCOPED_ENUM_FLAGS(T) \
inline constexpr T operator|(T lhs, T rhs) \
{ \
	return T( \
		std::underlying_type<T>::type(lhs) \
		| std::underlying_type<T>::type(rhs)); \
} \
inline constexpr T operator&(T lhs, T rhs) \
{ \
	return T( \
		std::underlying_type<T>::type(lhs) \
		& std::underlying_type<T>::type(rhs)); \
}
#endif

class SpinLock
{
	std::atomic<bool> m_lock = false;

public:
	void Lock()
	{
		while ( m_lock.exchange( true, std::memory_order_acquire ) )
		{
			//noop
		}
	}
	void Unlock()
	{
		m_lock.store( false, std::memory_order_release );
	}

	~SpinLock()
	{
		Unlock();
	}
};

class ScopedSpinLock
{
	SpinLock& m_spinlock;

public:
	ScopedSpinLock( SpinLock& lock ) : m_spinlock( lock )
	{
		m_spinlock.Lock();
	}

	~ScopedSpinLock()
	{
		m_spinlock.Unlock();
	}
};

inline uint64_t CalcAlignedSize( uint64_t size, uint64_t alignment )
{
	if ( alignment > 1 )
		return ( size + ( alignment - 1 ) ) & ~( alignment - 1 );
	return size;
}

static constexpr const size_t SizeKB = 1024;
static constexpr const size_t SizeMB = 1024 * SizeKB;

inline std::wstring ToWString( const char* str )
{
	std::wstring res =
		std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes( str );
	return res;
}

struct CorePaths
{
	std::string engine_content;
};

extern CorePaths g_core_paths;

inline std::string ToOSPath( const char* internal_path )
{
	// todo: move to core
	std::string result;

	if ( !internal_path )
		return result;

	static constexpr const char* engine_prefix = "#engine/";
	constexpr size_t engine_prefix_len = std::string_view( engine_prefix ).size();

	if ( !_strnicmp( internal_path, engine_prefix, engine_prefix_len ) )
	{
		result = g_core_paths.engine_content + ( internal_path + engine_prefix_len );
	}
	else
	{
		result = internal_path;
	}

	return result;
}