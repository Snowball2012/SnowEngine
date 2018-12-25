// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "ForwardCBProvider.h"


ForwardCBProvider::ForwardCBProvider( ID3D12Device& device, int n_bufferized_frames )
	: m_nbuffers( n_bufferized_frames )
{
	ThrowIfFailed( device.CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_UPLOAD ),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer( BufferGPUSize * m_nbuffers ),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS( m_gpu_res.GetAddressOf() ) ) );

	void* mapped_data = nullptr;
	ThrowIfFailed( m_gpu_res->Map( 0, nullptr, &mapped_data ) );

	m_mapped_data = span<uint8_t>( reinterpret_cast<uint8_t*>( mapped_data ),
								   reinterpret_cast<uint8_t*>( mapped_data ) + BufferGPUSize * m_nbuffers );
}


ForwardCBProvider::~ForwardCBProvider() noexcept
{
	if ( m_mapped_data.cbegin() && m_gpu_res )
		m_gpu_res->Unmap( 0, nullptr );
}


void ForwardCBProvider::Update( const Camera::Data& camera, const span<const SceneLight>& scene_lights )
{
	// ToDo: fill time info
	++m_cur_res_idx %= m_nbuffers; //-V567

	PassConstants gpu_data;
	FillCameraData( camera, gpu_data );
	FillLightData( scene_lights,
				   DirectX::XMLoadFloat4x4( &gpu_data.InvView ),
				   DirectX::XMMatrixTranspose( DirectX::XMLoadFloat4x4( &gpu_data.View ) ),
				   gpu_data );
	
	memcpy( m_mapped_data.begin() + BufferGPUSize * m_cur_res_idx, &gpu_data, BufferGPUSize );
}


D3D12_GPU_VIRTUAL_ADDRESS ForwardCBProvider::GetCBPointer() const noexcept
{
	return m_gpu_res->GetGPUVirtualAddress() + BufferGPUSize * m_cur_res_idx;
}


void ForwardCBProvider::FillCameraData( const Camera::Data& camera, PassConstants& gpu_data ) const noexcept
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
	DirectX::XMStoreFloat4x4( &gpu_data.Proj, DirectX::XMMatrixTranspose( proj ) );
	DirectX::XMStoreFloat4x4( &gpu_data.View, DirectX::XMMatrixTranspose( view ) );
	DirectX::XMStoreFloat4x4( &gpu_data.ViewProj, DirectX::XMMatrixTranspose( view_proj ) );

	DirectX::XMStoreFloat4x4( &gpu_data.InvProj, DirectX::XMMatrixTranspose( inv_proj ) );
	DirectX::XMStoreFloat4x4( &gpu_data.InvView, DirectX::XMMatrixTranspose( inv_view ) );
	DirectX::XMStoreFloat4x4( &gpu_data.InvViewProj, DirectX::XMMatrixTranspose( inv_view_proj ) );

	gpu_data.AspectRatio = camera.aspect_ratio;
	gpu_data.EyePosW = camera.pos;

	// reversed z
	gpu_data.FarZ = camera.near_plane;
	gpu_data.NearZ = camera.far_plane;
	gpu_data.FovY = camera.fov_y;
}


void ForwardCBProvider::FillLightData( const span<const SceneLight>& lights,
									   const DirectX::XMMATRIX& inv_view_matrix_transposed,
									   const DirectX::XMMATRIX& view_matrix,
									   PassConstants& gpu_data ) const
{
	// sort lights by type

	for ( const auto& light : lights )
	{
		if ( ! light.IsEnabled() )
			continue;
		switch ( light.GetData().type )
		{
			case SceneLight::LightType::Parallel:
				gpu_data.n_parallel_lights++;
				break;
			case SceneLight::LightType::Point:
				gpu_data.n_point_lights++;
				break;
			case SceneLight::LightType::Spotlight:
				gpu_data.n_spotlight_lights++;
				break;
			default:
				NOTIMPL;
		}
	}

	if ( gpu_data.n_parallel_lights + gpu_data.n_point_lights + gpu_data.n_spotlight_lights > MaxLights )
		throw SnowEngineException( "too many lights" );

	size_t parallel_idx = 0;
	size_t point_idx = gpu_data.n_parallel_lights;
	size_t spotlight_idx = gpu_data.n_spotlight_lights;

	for ( const auto& light : lights )
	{
		if ( ! light.IsEnabled() )
			continue;

		size_t gpu_idx = 0;
		switch ( light.GetData().type )
		{
			case SceneLight::LightType::Parallel:
				gpu_idx = parallel_idx++;
				break;
			case SceneLight::LightType::Point:
				gpu_idx = point_idx++;
				break;
			case SceneLight::LightType::Spotlight:
				gpu_idx = spotlight_idx++;
				break;
			default:
				NOTIMPL;
		};
		LightConstants& data = gpu_data.lights[gpu_idx];
		const auto& shadow_matrix = light.ShadowMatrix(); // here matrix is in world space, we need to convert it to view space for the shaders
		if ( shadow_matrix.has_value() )
		{
			DirectX::XMStoreFloat4x4( &data.shadow_map_matrix,
									  DirectX::XMMatrixMultiply( DirectX::XMMatrixTranspose( shadow_matrix.value() ), inv_view_matrix_transposed ) );
		}

		const SceneLight::Data& src_data = light.GetData();
		DirectX::XMStoreFloat3( &data.dir, DirectX::XMVector3TransformNormal( DirectX::XMLoadFloat3( &src_data.dir ), view_matrix ) );
		data.falloff_end = src_data.falloff_end;
		data.falloff_start = src_data.falloff_start;
		data.origin = src_data.origin;
		data.spot_power = src_data.spot_power;
		data.strength = src_data.strength;
	}
}
