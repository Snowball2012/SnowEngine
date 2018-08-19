#pragma once

#include "stdafx.h"

#include <tuple>

#include "RenderData.h"

// resource handles here must be lightweight. Try not to store the data itself here, only copyable handles/pointers with default constructors
struct ShadowCasters
{
	std::vector<RenderItem>* casters = nullptr;
};

struct ShadowProducers
{
	std::vector<Light>* lights = nullptr;
	std::vector<D3D12_GPU_VIRTUAL_ADDRESS>* pass_cbs = nullptr;
};

struct ShadowMaps
{

	std::vector<Light>* light_with_sm = nullptr;
};

struct ShadowMapStorage
{
	std::vector<Light>* sm_storage = nullptr;
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
	D3D12_CPU_DESCRIPTOR_HANDLE rtv;
};

struct HDRColorOut
{};

struct TonemapSettings
{};

struct BackbufferStorage
{};

struct TonemappedBackbuffer
{};

struct DepthStorage
{
	D3D12_CPU_DESCRIPTOR_HANDLE dsv;
};

struct FinalSceneDepth
{};

struct ScreenConstants
{
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
{};