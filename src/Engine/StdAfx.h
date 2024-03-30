#pragma once

#include <Core/Core.h>
#include <RHI/RHI.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_Vulkan.h>
#include <SDL2/SDL_syswm.h>

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/error/en.h>

#include <tinyexr/tinyexr.h>

#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

#define SDL_VERIFY(x) VERIFY_EQUALS(x, SDL_TRUE)

SE_LOG_CATEGORY( Engine );

struct EngineGlobals
{
    class RHI* rhi;
    class AssetManager* asset_mgr = nullptr;
    class Console* console = nullptr;
    class Renderer* renderer = nullptr;
};

extern EngineGlobals g_engine;

inline RHI& GetRHI() { return *g_engine.rhi; }
inline AssetManager& GetAssetManager() { return *g_engine.asset_mgr; }
inline Console& GetConsole() { return *g_engine.console; }
inline Renderer& GetRenderer() { return *g_engine.renderer; }