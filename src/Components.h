#pragma once

#include "SceneItems.h"

struct Transform
{
    DirectX::XMMATRIX local2world;
};

struct DrawableMesh
{
    StaticSubmeshID mesh = StaticSubmeshID::nullid;
    MaterialID material = MaterialID::nullid;
    bool show = true;
};
