#pragma once

#include <Core/Core.h>

#include <vulkan/vulkan.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_Vulkan.h>
#include <SDL2/SDL_syswm.h>

#define SDL_VERIFY(x) VERIFY_EQUALS(x, SDL_TRUE)

#include "stb_image.h"
