#pragma once

class ParallelSplitShadowMapping
{
public:
	// returned positions are sorted, near_z and far_z are clipping planes in view space
	// uniform_factor is an interpolation factor between logarithmic and uniform split schemes
	// positions[] must have space for at least splits_cnt - 1 elements
	static void CalcSplitPositionsVS( float near_z, float far_z, uint32_t splits_cnt, float uniform_factor, float positions[] ) noexcept;
private:
};