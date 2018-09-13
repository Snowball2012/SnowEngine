#pragma once

#include "stdafx.h"

#include <tuple>

#include "RenderData.h"
#include "ToneMappingPass.h"

// resource handles here must be lightweight. Try not to store the data itself here, only copyable handles/pointers with default constructors
struct ShadowCasters
{
	std::vector<RenderItem>* casters = nullptr;
};

struct ShadowProducers
{
	std::vector<Light*>* lights = nullptr;
	std::vector<D3D12_GPU_VIRTUAL_ADDRESS>* pass_cbs = nullptr;
};

struct ShadowMaps
{
	std::vector<Light*>* light_with_sm = nullptr;
};

struct ShadowMapStorage
{
	std::vector<Light*>* sm_storage = nullptr;
};

struct ObjectConstantBuffer
{
	ID3D12Resource* buffer = nullptr;
};

struct ForwardPassCB
{
	D3D12_GPU_VIRTUAL_ADDRESS pass_cb;
};

struct HDRColorStorage
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

struct TonemapNodeSettings
{
	ToneMappingPass::ShaderData data;
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

struct SceneContext
{
	RenderSceneContext* scene;
};

struct FrameResources
{

};

struct PreviousFrameStorage
{};

struct PreviousFrame
{};

struct NewPreviousFrame
{};

struct FinalBackbuffer
{
	D3D12_CPU_DESCRIPTOR_HANDLE rtv;
};