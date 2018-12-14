#pragma once

#include "RenderUtils.h"

#include <DirectXCollision.h>

#include "DescriptorHeap.h"

#include "DescriptorTableBakery.h"

#include "ResizableTexture.h"

#include "Scene.h"

// resides only in GPU mem
class DynamicTexture : public ResizableTexture
{
public:
	DynamicTexture( ComPtr<ID3D12Resource>&& texture, ID3D12Device* device,
					D3D12_RESOURCE_STATES initial_state, const D3D12_CLEAR_VALUE* opt_clear_value ) noexcept
		: ResizableTexture( std::move( texture ), device, initial_state, opt_clear_value )
	{}

	DescriptorTableBakery::TableID SRV() const { return m_srv; }
	DescriptorTableBakery::TableID UAV() const { return m_uav; }
	const Descriptor* RTV() const { return m_rtv.get(); }

	DescriptorTableBakery::TableID& SRV() { return m_srv; }
	DescriptorTableBakery::TableID& UAV() { return m_uav; }
	std::unique_ptr<Descriptor>& RTV() { return m_rtv; }

private:
	DescriptorTableBakery::TableID m_srv = DescriptorTableBakery::TableID::nullid;
	DescriptorTableBakery::TableID m_uav = DescriptorTableBakery::TableID::nullid;
	std::unique_ptr<Descriptor> m_rtv = nullptr;
};

struct RenderItem
{
	D3D12_VERTEX_BUFFER_VIEW vbv;
	D3D12_INDEX_BUFFER_VIEW ibv;
	uint32_t index_count = 0;
	uint32_t index_offset = 0;
	uint32_t vertex_offset = 0;
	
	D3D12_GPU_VIRTUAL_ADDRESS mat_cb;
	D3D12_GPU_DESCRIPTOR_HANDLE mat_table;

	D3D12_GPU_VIRTUAL_ADDRESS tf_addr;
};

struct ShadowMapGenData
{
	D3D12_GPU_VIRTUAL_ADDRESS pass_cb;
	D3D12_VIEWPORT viewport;
};

struct ShadowProducer
{
	ShadowMapGenData map_data;
	std::vector<RenderItem> casters;
};

struct ObjectConstants
{
	DirectX::XMFLOAT4X4 model = Identity4x4;
	DirectX::XMFLOAT4X4 model_inv_transpose = Identity4x4;
};

struct MaterialConstants
{
	DirectX::XMFLOAT4X4 mat_transform;
	DirectX::XMFLOAT3 diffuse_fresnel;
};

struct LightConstants
{
	DirectX::XMFLOAT4X4 shadow_map_matrix;
	DirectX::XMFLOAT3 strength; // spectral irradiance for parallel lights, in watt/sq.meter
	float falloff_start; // point and spotlight
	DirectX::XMFLOAT3 origin; // point and spotlight
	float falloff_end; // point and spotlight
	DirectX::XMFLOAT3 dir; // spotlight and parallel, direction TO the light source
	float spot_power; // spotlight only
};

constexpr int MAX_LIGHTS = 16;

struct PassConstants
{
	DirectX::XMFLOAT4X4 View = Identity4x4;
	DirectX::XMFLOAT4X4 InvView = Identity4x4;
	DirectX::XMFLOAT4X4 Proj = Identity4x4;
	DirectX::XMFLOAT4X4 InvProj = Identity4x4;
	DirectX::XMFLOAT4X4 ViewProj = Identity4x4;
	DirectX::XMFLOAT4X4 InvViewProj = Identity4x4;

	DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	float cbPerObjectPad1 = 0.0f;

	DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };

	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float FovY = 0.0f;
	float AspectRatio = 0.0f;

	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;
	DirectX::XMFLOAT2 _padding;

	LightConstants lights[MAX_LIGHTS];
	int n_parallel_lights = 0;
	int n_point_lights = 0;
	int n_spotlight_lights = 0;

	int use_linear_depth = 0;
};

// scene representation for renderer
struct RenderSceneContext
{
	std::unordered_map<std::string, StaticSubmeshID> static_geometry;

	// lighting, materials and textures
	std::unordered_map<std::string, MaterialID> materials;
	std::unordered_map<std::string, TextureID> textures;
};

using InputLayout = std::vector<D3D12_INPUT_ELEMENT_DESC>;
