#pragma once

#include <Core/Core.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_Vulkan.h>
#include <SDL2/SDL_syswm.h>

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>

using Json = rapidjson::Document;
using JsonValue = rapidjson::Value;

#define SDL_VERIFY(x) VERIFY_EQUALS(x, SDL_TRUE)