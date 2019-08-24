// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "ForwardCBProvider.h"

#include "ParallelSplitShadowMapping.h"


ForwardCBProvider::ForwardCBProvider()
{}


ForwardCBProvider ForwardCBProvider::Create( const Camera::Data& camera, const ParallelSplitShadowMapping& pssm,
                                             const span<const Light>& scene_lights,
                                             ID3D12Device& device, GPULinearAllocator& upload_cb_allocator )
{
    ForwardCBProvider buffer;

    auto allocation = upload_cb_allocator.Alloc( BufferGPUSize );

    ThrowIfFailedH( device.CreatePlacedResource(
        allocation.first, allocation.second,
        &CD3DX12_RESOURCE_DESC::Buffer( BufferGPUSize ),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS( buffer.m_gpu_res.GetAddressOf() ) ) );

    void* mapped_data = nullptr;
    ThrowIfFailedH( buffer.m_gpu_res->Map( 0, nullptr, &mapped_data ) );

    GPUPassConstants gpu_data;
    FillCameraData( camera, gpu_data );
    buffer.FillLightData( scene_lights,
                   DirectX::XMLoadFloat4x4( &gpu_data.view_inv_mat ),
                   DirectX::XMMatrixTranspose( DirectX::XMLoadFloat4x4( &gpu_data.view_mat ) ),
                   gpu_data );

    FillCSMData( camera, pssm, gpu_data );
    
    memcpy( mapped_data, &gpu_data, BufferGPUSize );

    buffer.m_gpu_res->Unmap( 0, nullptr );

    return buffer;
}


ComPtr<ID3D12Resource> ForwardCBProvider::GetResource() const noexcept
{
    return m_gpu_res;
}


span<const LightInCB> ForwardCBProvider::GetLightsInCB() const noexcept
{
    return make_span( m_lights_in_cb );
}


void ForwardCBProvider::FillCameraData( const Camera::Data& camera, GPUPassConstants& gpu_data ) noexcept
{
    // reversed z
    DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH( camera.fov_y,
                                                                camera.aspect_ratio,
                                                                camera.far_plane,
                                                                camera.near_plane );

    DirectX::XMMATRIX view = DirectX::XMMatrixLookToLH( DirectX::XMLoadFloat3( &camera.pos ),
                                                        DirectX::XMLoadFloat3( &camera.dir ),
                                                        DirectX::XMLoadFloat3( &camera.up ) );

    DirectX::XMMATRIX view_proj = view * proj;

    DirectX::XMVECTOR determinant;
    DirectX::XMMATRIX inv_view = DirectX::XMMatrixInverse( &determinant, view );
    DirectX::XMMATRIX inv_proj = DirectX::XMMatrixInverse( &determinant, proj );
    DirectX::XMMATRIX inv_view_proj = DirectX::XMMatrixInverse( &determinant, view_proj );

    // HLSL vectors are row vectors, so we need to transpose all the matrices before uploading them to gpu
    DirectX::XMStoreFloat4x4( &gpu_data.proj_mat, DirectX::XMMatrixTranspose( proj ) );
    DirectX::XMStoreFloat4x4( &gpu_data.view_mat, DirectX::XMMatrixTranspose( view ) );
    DirectX::XMStoreFloat4x4( &gpu_data.view_proj_mat, DirectX::XMMatrixTranspose( view_proj ) );

    DirectX::XMStoreFloat4x4( &gpu_data.proj_inv_mat, DirectX::XMMatrixTranspose( inv_proj ) );
    DirectX::XMStoreFloat4x4( &gpu_data.view_inv_mat, DirectX::XMMatrixTranspose( inv_view ) );
    DirectX::XMStoreFloat4x4( &gpu_data.view_proj_inv_mat, DirectX::XMMatrixTranspose( inv_view_proj ) );

    gpu_data.aspect_ratio = camera.aspect_ratio;
    gpu_data.eye_pos_w = camera.pos;

    // reversed z
    gpu_data.far_z = camera.near_plane;
    gpu_data.near_z = camera.far_plane;
    gpu_data.fov_y = camera.fov_y;
}


void ForwardCBProvider::FillLightData( const span<const Light>& lights,
                                       const DirectX::XMMATRIX& inv_view_matrix_transposed,
                                       const DirectX::XMMATRIX& view_matrix,
                                       GPUPassConstants& gpu_data )
{
    m_lights_in_cb.clear();

    // sort lights by type

    for ( const auto& light : lights )
    {
        if ( ! light.IsEnabled() )
            continue;

        const Light::LightType light_type = light.GetData().type;

        switch ( light_type )
        {
            case Light::LightType::Parallel:
                gpu_data.n_parallel_lights++;
                break;
            case Light::LightType::Point:
                gpu_data.n_point_lights++;
                break;
            case Light::LightType::Spotlight:
                gpu_data.n_spotlight_lights++;
                break;
            default:
                NOTIMPL;
        }
    }

    if ( gpu_data.n_parallel_lights + gpu_data.n_point_lights + gpu_data.n_spotlight_lights > MaxLights )
        throw SnowEngineException( "too many lights" );

    uint32_t parallel_idx = 0;
    uint32_t point_idx = 0;
    uint32_t spotlight_idx = gpu_data.n_point_lights;

    for ( const auto& light : lights )
    {
        if ( ! light.IsEnabled() )
            continue;

        const Light::LightType light_type = light.GetData().type;

        uint32_t gpu_idx = 0;
        switch ( light_type )
        {
            case Light::LightType::Parallel:
                gpu_idx = parallel_idx++;
                break;
            case Light::LightType::Point:
                gpu_idx = point_idx++;
                break;
            case Light::LightType::Spotlight:
                gpu_idx = spotlight_idx++;
                break;
            default:
                NOTIMPL;
        };

        m_lights_in_cb.push_back( LightInCB{ gpu_idx, &light } );

        if ( light_type == Light::LightType::Parallel )
        {
            ParallelLightConstants& data = gpu_data.parallel_lights[gpu_idx];
            const auto& shadow_matrices = light.GetShadowMatrices(); // here matrix is in world space, we need to convert it to view space for the shaders
            if ( shadow_matrices.size() > MAX_CASCADE_SIZE )
                throw SnowEngineException( "too many matrices for csm" );
            for ( int i = 0; i < shadow_matrices.size(); ++i )
            {
                DirectX::XMStoreFloat4x4( &data.shadow_map_matrix[i],
                                          DirectX::XMMatrixMultiply( DirectX::XMMatrixTranspose( shadow_matrices[i] ), inv_view_matrix_transposed ) );
            }
            data.csm_num_split_positions = int32_t( shadow_matrices.size() ) - 1;

            const Light::Data& src_data = light.GetData();
            DirectX::XMStoreFloat3( &data.dir, DirectX::XMVector3TransformNormal( DirectX::XMLoadFloat3( &src_data.dir ), view_matrix ) );
            data.strength = src_data.strength;
        }
        else
        {
            LightConstants& data = gpu_data.lights[gpu_idx];
            const auto& shadow_matrix = light.GetShadowMatrices(); // here matrix is in world space, we need to convert it to view space for the shaders
            if ( shadow_matrix.size() == 1 )
            {
                DirectX::XMStoreFloat4x4( &data.shadow_map_matrix,
                                          DirectX::XMMatrixMultiply( DirectX::XMMatrixTranspose( shadow_matrix[0] ), inv_view_matrix_transposed ) );
            }

            const Light::Data& src_data = light.GetData();
            DirectX::XMStoreFloat3( &data.dir, DirectX::XMVector3TransformNormal( DirectX::XMLoadFloat3( &src_data.dir ), view_matrix ) );
            data.falloff_end = src_data.falloff_end;
            data.falloff_start = src_data.falloff_start;
            data.origin = src_data.origin;
            data.spot_power = src_data.spot_power;
            data.strength = src_data.strength;
        }
    }
}

void ForwardCBProvider::FillCSMData( const Camera::Data& camera, const ParallelSplitShadowMapping& pssm, GPUPassConstants& gpu_data ) noexcept
{
    // fill split positions
    const auto& res_positions = pssm.CalcSplitPositionsVS( camera, make_span( gpu_data.csm_split_positions, gpu_data.csm_split_positions + MAX_CASCADE_SIZE - 1 ) );
    for ( uint32_t i = uint32_t( res_positions.size() - 1 ); i > 0; --i )
        gpu_data.csm_split_positions[4 * i] = res_positions[i];
}
