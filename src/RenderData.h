#pragma once

#include <Luna/d3dUtil.h>

struct Vertex
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT4 color;
	DirectX::XMFLOAT3 normal;
};

struct MaterialConstants
{
	DirectX::XMFLOAT4X4 mat_transform;
	DirectX::XMFLOAT4 diffuse_albedo;
	DirectX::XMFLOAT3 fresnel_r0;
	float roughness;
};

struct StaticMaterial
{
	MaterialConstants mat_constants;

	Microsoft::WRL::ComPtr<ID3D12Resource> cb_gpu = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> cb_uploader = nullptr;

	void DisposeUploaders()
	{
		cb_uploader = nullptr;
	}
	void LoadToGPU( ID3D12Device& device, ID3D12GraphicsCommandList& cmd_list );
};

struct RenderItem
{
	// geom
	MeshGeometry* geom = nullptr;
	uint32_t index_count = 0;
	uint32_t index_offset = 0;
	uint32_t vertex_offset = 0;
	
	StaticMaterial* material = nullptr;

	// uniforms
	int cb_idx = -1;
	DirectX::XMFLOAT4X4 world_mat = MathHelper::Identity4x4();

	int n_frames_dirty = 0;
};

struct ObjectConstants
{
	DirectX::XMFLOAT4X4 model = MathHelper::Identity4x4();
};

struct LightConstants
{
	DirectX::XMFLOAT3 strength;
	float falloff_start; // point and spotlight
	DirectX::XMFLOAT3 origin; // point and spotlight
	float falloff_end; // point and spotlight
	DirectX::XMFLOAT3 dir; // spotlight and parallel, direction TO the light source
	float spot_power; // spotlight only
};

struct Light
{
	enum class Type
	{
		Ambient,
		Parallel,
		Point,
		Spotlight
	} type;

	LightConstants data;
};

constexpr int MAX_LIGHTS = 16;

struct PassConstants
{
	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	float cbPerObjectPad1 = 0.0f;
	DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;

	LightConstants lights[MAX_LIGHTS];
	int n_parallel_lights = 0;
	int n_point_lights = 0;
	int n_spotlight_lights = 0;
};