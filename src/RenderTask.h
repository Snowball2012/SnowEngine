#pragma once

#include "ForwardCBProvider.h"

enum class RenderMode
{
    FullTonemapped
};

class RenderTask
{
public:

    RenderTask( const RenderTask& ) = delete;
    RenderTask& operator=( const RenderTask& ) = delete;
    RenderTask( RenderTask&& ) = default;
    RenderTask& operator=( RenderTask&& ) = default;

    struct Frustrum
    {
        enum class Type
        {
            Perspective,
            Orthographic
        } type;
        DirectX::XMMATRIX proj;
        DirectX::XMMATRIX view;
        DirectX::XMMATRIX viewproj;
    };

    struct ShadowFrustrum
    {
        Frustrum frustrum;
        Light* light;
    };

    const Frustrum& GetMainPassFrustrum() const;;

    const span<const ShadowFrustrum> GetShadowFrustrums() const;

    void AddShadowBatches( span<const RenderBatch> batches, uint32_t list_idx );
    void AddOpaqueBatches( span<const RenderBatch> batches );

private:

    friend class Renderer;

    RenderTask( ForwardCBProvider&& forward_cb );

    std::vector<std::vector<span<const RenderBatch>>> m_shadow_renderlists;
    std::vector<span<const RenderBatch>> m_opaque_renderlist;

    Frustrum m_main_frustrum;
    std::vector<ShadowFrustrum> m_shadow_frustrums;

    RenderMode m_mode;

    Camera::Data m_main_camera;

    ForwardCBProvider m_forward_cb_provider;
};