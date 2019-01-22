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

struct ShadowMapStorage
{
	D3D12_CPU_DESCRIPTOR_HANDLE dsv;
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
	ID3D12Resource* res;
};

struct ShadowMaps
{
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
};

struct ShadowCascadeStorage
{
	D3D12_CPU_DESCRIPTOR_HANDLE dsv;
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
	ID3D12Resource* res;
};

struct ShadowCascade
{
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
};

struct ForwardPassCB
{
	D3D12_GPU_VIRTUAL_ADDRESS pass_cb;
};

struct HDRDirectStorage
{
	ID3D12Resource* resource;
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv;
};

struct HDRDirect
{
	ID3D12Resource* resource;
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv;
};

struct HDRColorOut
{
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
};

struct SSAmbientLightingStorage
{
	ID3D12Resource* resource;
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv;
};

struct SSAmbientLighting
{
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
};

struct Skybox
{
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
};

struct SSNormalStorage
{
	ID3D12Resource* resource;
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv;
};

struct SSNormals
{
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
};

struct SSAOStorage
{
	ID3D12Resource* resource;
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv;
};

struct SSAOTexture_Noisy
{
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
};

struct SSAOStorage_Blurred
{
	ID3D12Resource* resource;
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
	D3D12_GPU_DESCRIPTOR_HANDLE uav;
};

struct SSAOStorage_BlurredHorizontal
{
	ID3D12Resource* resource;
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
	D3D12_GPU_DESCRIPTOR_HANDLE uav;
};

struct SSAOTexture_Blurred
{
	D3D12_GPU_DESCRIPTOR_HANDLE srv;	
};

struct TonemapNodeSettings
{
	ToneMappingPass::ShaderData data;
};

struct HBAOSettings
{
	HBAOPass::Settings data;
};

struct BackbufferStorage
{
	ID3D12Resource* resource;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv;
};

struct TonemappedBackbuffer
{
	ID3D12Resource* resource;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv;
};

struct DepthStorage
{
	D3D12_CPU_DESCRIPTOR_HANDLE dsv;
	D3D12_GPU_DESCRIPTOR_HANDLE srv;
};

struct FinalSceneDepth
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

struct FinalBackbuffer
{
	D3D12_CPU_DESCRIPTOR_HANDLE rtv;
};

struct ImGuiFontHeap
{
	ID3D12DescriptorHeap* heap = nullptr;
};
