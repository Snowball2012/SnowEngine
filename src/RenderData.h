#pragma once

#include <Luna/d3dUtil.h>

struct Vertex
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT4 color;
	DirectX::XMFLOAT3 normal;
};

struct RenderItem
{
	// geom
	MeshGeometry* geom = nullptr;
	uint32_t index_count = 0;
	uint32_t index_offset = 0;
	uint32_t vertex_offset = 0;

	// uniforms
	int cb_idx = 0;
	DirectX::XMFLOAT4X4 world_mat = MathHelper::Identity4x4();

	int n_frames_dirty = 0;
};

struct ObjectConstants
{
	DirectX::XMFLOAT4X4 model = MathHelper::Identity4x4();
};

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
};
