#pragma once

#include "utils/span.h"

#include "Ptr.h"

#include "SceneItems.h"

class ParallelSplitShadowMapping
{
public:
	// returned positions are sorted, near_z and far_z are clipping planes in view space
	// uniform_factor is an interpolation factor between logarithmic and uniform split schemes
	// positions_storage.size() determines the number of splits (splits_num = position_starage.size() + 1)
	static void CalcSplitPositionsVS( float near_z, float far_z, float uniform_factor, const span<float>& positions_storage ) noexcept;

	// frustrum_vertices must contain exactly 8 vertices
	static void CalcShadowMatrixForFrustrumLH( const span<DirectX::XMVECTOR>& frustrum_vertices,
	                                           const DirectX::XMVECTOR& light_dir, float additional_height,
											   DirectX::XMMATRIX& shadow_matrix ) noexcept;

	// positions_storage and matrices_storage must have enough space to contain up to
	// MAX_CASCADE_SIZE - 1 and MAX_CASCADE_SIZE elements respectively
	// returns subrange of the source span
	span<float> CalcSplitPositionsVS( const Camera::Data& camera_data, const span<float>& positions_storage ) const noexcept;

	// light must be parallel, split_positions must be initialized
	span<DirectX::XMMATRIX> CalcShadowMatricesWS( const Camera::Data& camera, const SceneLight& light, const span<float>& split_positions, span<DirectX::XMMATRIX> matrices_storage ) const;

	// settings
	void SetUniformFactor( float uniform_factor ) noexcept;
	float GetUniformFactor() const noexcept { return m_uniform_factor; }

	void SetSplitsNum( uint32_t splits_num ) noexcept { m_splits_num = splits_num; }
	uint32_t GetSplitsNum() const noexcept { return m_splits_num; }

private:
	float m_uniform_factor = 0.3f;
	uint32_t m_splits_num = 1;
};