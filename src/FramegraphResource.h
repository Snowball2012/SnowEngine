#pragma once

#include "stdafx.h"

#include <tuple>

#include "RenderData.h"
#include "ToneMappingPass.h"
#include "HBAOPass.h"

// resource handles here must be lightweight. Try not to store the data itself here, only copyable handles/pointers with default constructors

struct ShadowProducers
{
	span<ShadowProducer> arr;
};

struct ShadowCascadeProducers
{
	span<ShadowCascadeProducer> arr;
};

struct ShadowMaps
{
	ID3D12Resource* res;
	D3D12_CPU_DESCRIPTOR_HANDLE dsv;
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
};

struct ShadowCascade
{
	ID3D12Resource* res;
	D3D12_CPU_DESCRIPTOR_HANDLE dsv;
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
};

struct ForwardPassCB
{
	D3D12_GPU_VIRTUAL_ADDRESS pass_cb;
};

struct HDRBuffer
{
	ID3D12Resource* res;
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv;
};

struct AmbientBuffer
{
	ID3D12Resource* res;
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv;
};

struct NormalBuffer
{
	ID3D12Resource* res;
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv;
};

struct Skybox
{
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
	D3D12_GPU_VIRTUAL_ADDRESS tf_cbv;
	float radiance_factor;
};

struct SSAOBuffer_Noisy
{
	ID3D12Resource* res;
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv;
};

struct SSAOTexture_Blurred
{
	ID3D12Resource* res;
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
	D3D12_GPU_DESCRIPTOR_HANDLE uav;
};

struct SSAOTexture_Transposed
{
	ID3D12Resource* res;
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
	D3D12_GPU_DESCRIPTOR_HANDLE uav;
};


struct TonemapNodeSettings
{
	ToneMappingPass::ShaderData data;
};

struct HBAOSettings
{
	HBAOPass::Settings data;
};

struct SDRBuffer
{
	ID3D12Resource* resource;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv;
};

struct DepthStencilBuffer
{
	D3D12_CPU_DESCRIPTOR_HANDLE dsv;
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
};

struct ScreenConstants
{
	D3D12_VIEWPORT viewport;
	D3D12_RECT scissor_rect;
};

struct MainRenderitems
{
	span<RenderItem> items;
};

struct ImGuiFontHeap
{
	ID3D12DescriptorHeap* heap = nullptr;
};
