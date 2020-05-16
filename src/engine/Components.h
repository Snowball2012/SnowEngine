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

class Camera
{
public:
    enum class Type
    {
        Perspective,
        Orthographic
    };
    struct Data
    {
        DirectX::XMFLOAT3 pos;
        DirectX::XMFLOAT3 dir;
        DirectX::XMFLOAT3 up;
        float aspect_ratio;
        union
        {
            float fov_y;
            float height;
        };
        float near_plane;
        float far_plane;
        Type type;
    };
    const Data& GetData() const noexcept { return m_data; }
    Data& ModifyData() noexcept { return m_data; }

    EnvMapID GetSkybox() const noexcept { return m_skybox; }
    EnvMapID& SetSkybox() noexcept { return m_skybox; }

private:
    Data m_data;
    EnvMapID m_skybox = EnvMapID::nullid;
};