#pragma once

#define NOMINMAX

#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#include <atomic>
#include <array>
#include <algorithm>
#include <fstream>
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
#include <unordered_set>

#include <boost/container/flat_map.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/scope_exit.hpp>

namespace bc = boost::container;

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

#define VERIFY( x ) \
if ((x) == (false)) \
{ \
	INSERT_DBGBREAK \
	throw std::runtime_error( "Failed call to " #x " at " __FILE__ ":" S_(__LINE__) ); \
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
inline constexpr T& operator|=(T& lhs, T rhs) \
{ \
	lhs = lhs | rhs; \
	return lhs; \
} \
inline constexpr T operator&(T lhs, T rhs) \
{ \
	return T( \
		std::underlying_type<T>::type(lhs) \
		& std::underlying_type<T>::type(rhs)); \
} \
inline constexpr T& operator&=( T& lhs, T rhs ) \
{ \
	lhs = lhs & rhs; \
	return lhs; \
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

// -1 converts any unsigned type to max value (all bits raised)
#define MASK_BITS_ALL -1
#define MASK_BITS_NONE 0

// Allows to easily memcpy unrelated types
template< typename T1, typename T2 >
void MemcpyUnrelatedSafe( T1& dst, T2& src )
{
	static_assert( sizeof( dst ) == sizeof( src ), "can't memcpy structs with different sizes" );
	memcpy( reinterpret_cast< void* >( &dst ), reinterpret_cast< const void* >( &src ), sizeof( dst ) );
}

inline std::wstring ToWString( const char* str )
{
	std::wstring res =
		std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes( str );
	return res;
}

// Explicitly instantiate SetFromString for necessary types. Used for console variables and a bunch of other systems
inline bool SetFromString( const char* str, int& val )
{
	if ( str == nullptr )
		return false;

	// @todo - undefined behaviour when str does not represent an integer
	val = atoi( str );
	return true;
}

// Explicitly instantiate SetFromString for necessary types. Used for console variables and a bunch of other systems
inline bool SetFromString( const char* str, uint32_t& val )
{
	if ( str == nullptr )
		return false;

	val = strtoul( str, nullptr, 10 );

	if ( val == 0 )
	{
		if ( errno == EINVAL || errno == ERANGE )
		{
			return false;
		}
	}
	return true;
}

// Explicitly instantiate SetFromString for necessary types. Used for console variables and a bunch of other systems
inline bool SetFromString( const char* str, bool& val )
{
	if ( str == nullptr )
		return false;

	if ( !_stricmp( str, "true" ) )
	{
		val = true;
		return true;
	}

	if ( !_stricmp( str, "false" ) )
	{
		val = false;
		return true;
	}

	return false;
}

struct CorePaths
{
	std::string engine_content;
};

extern CorePaths g_core_paths;

std::string ToOSPath( const char* internal_path );

inline void HandleFatalError()
{
	// Just crash immediately for now
	bool fatal_error_encountered = false;
	VERIFY( fatal_error_encountered );
}

struct LogCategory
{
	const char* name = "";
	bool enabled = true;
};

enum class LogMessageType
{
	Info = 0,
	Warning,
	Error,				// Recoverable content errors. Do not use this for code errors, we have SE_ENSURE for that
	FatalError			// Unrecoverable content errors
};

class Logger
{
private:
	std::optional<std::ofstream> m_file;
	bool m_write_to_std = true;
	bool m_write_debugstr = true;

public:
	struct CreateInfo
	{
		const char* path = nullptr;
		bool mirror_to_stdout = true;
	};

	Logger( const CreateInfo& info );
	~Logger();
	void Log( const LogCategory& category, LogMessageType msg_type, const char* fmt, ... );
	void Newline();
	void Flush();

private:
	static const char* TypeToString( LogMessageType type );
};

extern Logger* g_log;

#define SE_LOG( category, type, fmt, ... ) g_log->Log( g_logcategory_ ## category, LogMessageType::type, fmt, __VA_ARGS__ )

#define SE_LOG_FATAL_ERROR( category, fmt, ... ) g_log->Log( g_logcategory_ ## category, LogMessageType::FatalError, fmt, __VA_ARGS__ )
#define SE_LOG_ERROR( category, fmt, ... ) g_log->Log( g_logcategory_ ## category, LogMessageType::Error, fmt, __VA_ARGS__ )
#define SE_LOG_WARNING( category, fmt, ... ) g_log->Log( g_logcategory_ ## category, LogMessageType::Warning, fmt, __VA_ARGS__ )
#define SE_LOG_INFO( category, fmt, ... ) g_log->Log( g_logcategory_ ## category, LogMessageType::Info, fmt, __VA_ARGS__ )
#define SE_LOG_NEWLINE() g_log->Newline()

#define SE_LOG_CATEGORY( category ) inline LogCategory g_logcategory_ ## category { .name = S_(category) }
#define SE_LOG_CATEGORY_EX( category, enableByDefault ) inline LogCategory g_logcategory_ ## category { .name = S_( category ), .enabled = ( enableByDefault ) }

SE_LOG_CATEGORY( Core );

SE_LOG_CATEGORY( Temp );
