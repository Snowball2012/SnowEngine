#pragma once

#include <assert.h>

#define SE_ASSERT( expr ) assert( expr )
#define SE_ENSURE( expr ) \
	((!!(expr)) || \
    (_wassert(_CRT_WIDE(#expr), _CRT_WIDE(__FILE__), (unsigned)(__LINE__)), 0))