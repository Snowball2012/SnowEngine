// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "TemporalAA.h"

using namespace DirectX;

void TemporalAA::FillShaderData( TemporalBlendPass::ShaderData& data ) const
{
    data.color_window_size = m_color_window_size;
    data.prev_frame_blend_val = m_blend_feedback;
    if ( IsJitterEnabled() )
    {
        data.unjitter[0] = m_cur_jitter[0];
        data.unjitter[1] = -m_cur_jitter[1];
    }
    else
    {
        data.unjitter[0] = 0;
        data.unjitter[1] = 0;
    }
}

void TemporalAA::SetFrame( uint64_t frame_idx )
{
    int sample = frame_idx % 8;

    m_cur_jitter[0] = HaltonSequence23[sample][0] - 0.5f;
    m_cur_jitter[1] = HaltonSequence23[sample][1] - 0.5f;
}

DirectX::XMFLOAT4X4 TemporalAA::JitterProjection( const DirectX::XMFLOAT4X4& proj, unsigned int width, unsigned int height ) const
{
    XMFLOAT4X4 proj_jittered = proj;
    if ( m_jitter_proj )
    {
        const float sample_size_x = 2.0f * m_jitter_val / width;
        const float sample_size_y = 2.0f * m_jitter_val / height;

        proj_jittered( 2, 0 ) += m_cur_jitter[0] * sample_size_x;
        proj_jittered( 2, 1 ) += m_cur_jitter[1] * sample_size_y;
    }
    return proj_jittered;
}
