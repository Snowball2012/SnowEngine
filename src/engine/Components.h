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
        DirectX::XMFLOAT3 pos_prev;
        DirectX::XMFLOAT3 dir_prev;
        DirectX::XMFLOAT3 up_prev;
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
    void SavePrevFrameData() noexcept;

    EnvMapID GetSkybox() const noexcept { return m_skybox; }
    EnvMapID& SetSkybox() noexcept { return m_skybox; }

private:
    Data m_data;
    EnvMapID m_skybox = EnvMapID::nullid;
};

inline void Camera::SavePrevFrameData() noexcept
{
    m_data.pos_prev = m_data.pos;
    m_data.dir_prev = m_data.dir;
    m_data.up_prev = m_data.up;
}
