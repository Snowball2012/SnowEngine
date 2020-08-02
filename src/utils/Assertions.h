#pragma once

#include <assert.h>
#include <winerror.h>

#define SE_ASSERT( expr ) assert( expr )

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