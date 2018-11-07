#pragma once

#include <d3d12.h>
#include <wrl.h>

#include "utils/span.h"

#include "RenderData.h"
#include "SceneItems.h"

// Fills PassConstants circular buffer on GPU
class ForwardCBProvider
{
public:
	ForwardCBProvider( ID3D12Device& device, int n_bufferized_frames );
	~ForwardCBProvider() noexcept;

	void Update( const Camera::Data& camera, const span<const SceneLight>& scene_lights );

	D3D12_GPU_VIRTUAL_ADDRESS GetCBPointer() const noexcept;

private:
	void FillCameraData( const Camera::Data& camera, PassConstants& gpu_data ) const noexcept;
	void FillLightData( const span<const SceneLight>& lights, PassConstants& gpu_data ) const;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_gpu_res = nullptr;
	span<uint8_t> m_mapped_data;
	int m_cur_res_idx = 0;
	const int m_nbuffers;

	static constexpr UINT BufferGPUSize = Utils::CalcConstantBufferByteSize( sizeof( PassConstants ) );
	static constexpr size_t MaxLights = sizeof( PassConstants::lights ) / sizeof( LightConstants );
};