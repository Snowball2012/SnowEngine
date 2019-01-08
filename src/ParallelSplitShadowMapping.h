#pragma once

struct PassConstants;

class ParallelSplitShadowMapping
{
public:
	// returned positions are sorted, near_z and far_z are clipping planes in view space
	// uniform_factor is an interpolation factor between logarithmic and uniform split schemes
	// positions[] must have space for at least splits_cnt - 1 elements
	static void CalcSplitPositionsVS( float near_z, float far_z, uint32_t splits_cnt, float uniform_factor, float positions[] ) noexcept;

	//span<float> GetSplitPositions() const noexcept;

	// fills shadow matrices for the light
	//void CalcShadowMatrices( const SceneLight& light, const Camera::Data& camera, const DirectX::XMVECTOR& light_dir );

	void SetSplitCount( uint32_t split_cnt );
	void SetUniformFactor( float uniform_factor );

private:
	float m_uniform_factor;
	uint32_t m_splits_cnt;
	mutable std::vector<float> m_split_positions;
};