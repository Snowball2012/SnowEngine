#pragma once

#include <vulkan/vulkan.h>

#include <iostream>
#include <vector>
#include <optional>
#include <span>
#include <set>

#include <windows.h>

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