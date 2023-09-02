#pragma once

#include "StdAfx.h"

#include "Assets.h"

class DisplayMapping
{
private:

    TextureAssetPtr m_dbg_texture = nullptr;

public:

    void DebugUI();
};