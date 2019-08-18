#pragma once

#include <d3d12.h>
#include <wrl.h>

#include "utils/span.h"

#include "RenderData.h"
#include "SceneItems.h"

#include "GPUAllocators.h"

class ParallelSplitShadowMapping;

// Fills GPUPassConstants circular buffer on GPU
class ForwardCBProvider
{
public:
    static ForwardCBProvider Create( const Camera::Data& camera, const ParallelSplitShadowMapping& pssm,
                                     const span<const Light>& scene_lights,
                                     ID3D12Device& device, GPULinearAllocator& upload_cb_allocator );

    ComPtr<ID3D12Resource> GetResource() const noexcept;
    span<const LightInCB> GetLightsInCB() const noexcept;

private:
    static constexpr uint64_t BufferGPUSize = Utils::CalcConstantBufferByteSize( sizeof( GPUPassConstants ) );
    static constexpr size_t MaxLights = sizeof( GPUPassConstants::lights ) / sizeof( LightConstants );
    static constexpr size_t MaxParallelLights = sizeof( GPUPassConstants::parallel_lights ) / sizeof( ParallelLightConstants );
    
    Microsoft::WRL::ComPtr<ID3D12Resource> m_gpu_res = nullptr;
    bc::static_vector<LightInCB, MaxLights + MaxParallelLights> m_lights_in_cb;

    ForwardCBProvider( );

    static void FillCameraData( const Camera::Data& camera, GPUPassConstants& gpu_data ) noexcept;
    static void FillCSMData( const Camera::Data& camera, const ParallelSplitShadowMapping& pssm,
                             GPUPassConstants& gpu_data ) noexcept;

    // pass transposed matrix here, it's more convenient for a caller 
    void FillLightData( const span<const Light>& lights,
                        const DirectX::XMMATRIX& inv_view_matrix_transposed,
                        const DirectX::XMMATRIX& view_matrix,
                        GPUPassConstants& gpu_data );    

};