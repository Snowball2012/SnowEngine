#pragma once

#define NOMINMAX

#include <exception>
#include <stdexcept>
#include <vector>
#include <set>
#include <iostream>
#include <optional>
#include <algorithm>
#include <array>
#include <chrono>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_Vulkan.h>

#include "stb_image.h"

#include <windows.h>

// double extension trick to stringify __LINE__ and other things
#define S(x) #x
#define S_(x) S(x)

#define VERIFY_EQUALS(x, ExpectedValue) if ((x) != (ExpectedValue)) throw std::runtime_error("Failed call to " #x " at " __FILE__ ":" S_(__LINE__) ", expected " #ExpectedValue)

#define VERIFY_NOT_EQUAL(x, FailValue) if ((x) == (FailValue)) throw std::runtime_error("Failed call to " #x " at " __FILE__ ":" S_(__LINE__) ", expected not " #FailValue)

#define VK_VERIFY(x) VERIFY_EQUALS(x, VK_SUCCESS)

#define SDL_VERIFY(x) VERIFY_EQUALS(x, SDL_TRUE)

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

#include <wrl.h>

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;