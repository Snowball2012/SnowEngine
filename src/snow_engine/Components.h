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

    DirectX::XMFLOAT3 GenerateRayFromNDC( float x, float y ) const;

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

inline DirectX::XMFLOAT3 Camera::GenerateRayFromNDC( float x, float y ) const
{
    if ( m_data.type != Type::Perspective )
        NOTIMPL;

    DirectX::XMVECTOR forward = XMLoadFloat3( &m_data.dir );
    DirectX::XMVECTOR up = XMLoadFloat3( &m_data.up );
    DirectX::XMVECTOR right = DirectX::XMVector3Normalize( DirectX::XMVector3Cross( up, forward ) );
    up = DirectX::XMVector3Cross( forward, right );
    DirectX::XMFLOAT3 right3;
    XMStoreFloat3( &right3, right );
    DirectX::XMFLOAT3 up3;
    XMStoreFloat3( &up3, up );

    float half_height = std::tan( m_data.fov_y * 0.5 );
    float half_width = half_height * m_data.aspect_ratio;
    DirectX::XMFLOAT3 ray = m_data.dir + up3 * y * half_height + right3 * half_width * x;

    XMFloat3Normalize( ray );

    return ray;
}
