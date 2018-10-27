#pragma once

#include "RenderUtils.h"

#include <DirectXCollision.h>

#include "DescriptorHeap.h"

#include "DescriptorTableBakery.h"

#include "Scene.h"

struct SubmeshGeometry
{
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	INT BaseVertexLocation = 0;

	DirectX::BoundingBox Bounds;
};

struct MaterialConstants
{
	DirectX::XMFLOAT4X4 mat_transform;
	DirectX::XMFLOAT3 diffuse_fresnel;
};

struct GPUTexture
{
	Microsoft::WRL::ComPtr<ID3D12Resource> texture_gpu = nullptr;
	DescriptorTableBakery::TableID srv;
};

// resides only in GPU mem
struct DynamicTexture : public GPUTexture
{
	std::unique_ptr<Descriptor> rtv = nullptr;
};

struct ShadowMap : public GPUTexture
{
	std::unique_ptr<Descriptor> dsv = nullptr;
	D3D12_GPU_DESCRIPTOR_HANDLE frame_srv;
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

struct ObjectConstants
{
	DirectX::XMFLOAT4X4 model = Identity4x4;
	DirectX::XMFLOAT4X4 model_inv_transpose = Identity4x4;
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

struct GPULight
{
	enum class Type
	{
		Ambient,
		Parallel,
		Point,
		Spotlight
	} type;

	LightConstants data;

	ShadowMap* shadow_map;
	bool is_dynamic = false;
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
	float FovY;
	float AspectRatio;

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
	std::unordered_map<std::string, GPULight> lights;
	std::unordered_map<std::string, TextureID> textures;
	std::unordered_map<std::string, ShadowMap> shadow_maps;

	// camera
	DirectX::XMFLOAT4X4 view = Identity4x4;
	DirectX::XMFLOAT4X4 proj = Identity4x4;

	DirectX::XMFLOAT4X4 shadow_frustrum_proj = Identity4x4;
	DirectX::XMFLOAT4X4 shadow_frustrum_view = Identity4x4;

	DirectX::XMFLOAT4X4 main_frustrum_proj = Identity4x4;
	DirectX::XMFLOAT4X4 main_frustrum_view = Identity4x4;
};

using InputLayout = std::vector<D3D12_INPUT_ELEMENT_DESC>;