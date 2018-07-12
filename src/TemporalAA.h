#pragma once

// simple class that stores settings and state for Temporal Anti-Aliasing

#include "TemporalBlendPass.h"

class TemporalAA
{
public:
	TemporalAA() {};

	void FillShaderData( TemporalBlendPass::ShaderData& data ) const;
	void SetFrame( uint64_t frame_idx );
	DirectX::XMFLOAT4X4 JitterProjection( const DirectX::XMFLOAT4X4& proj, unsigned int width, unsigned int height ) const;

	bool IsJitterEnabled() const { return m_jitter_proj; }
	bool IsBlendEnabled() const { return m_blend_prev_frame; }
	
	bool& SetJitter() { return m_jitter_proj; }
	float& SetJitterVal() { return m_jitter_val; }
	bool& SetBlend() { return m_blend_prev_frame; }
	float& SetBlendFeedback() { return m_blend_feedback; }
	float& SetColorWindowExpansion() { return m_color_window_size; }

private:
	bool m_jitter_proj = true;
	float m_jitter_val = 1.0f;
	float m_color_window_size = 0.2f;
	bool m_blend_prev_frame = true;
	float m_blend_feedback = 0.9f;
	float m_cur_jitter[2] = { 0 };
};