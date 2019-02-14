#pragma once

#include "GPUTaskQueue.h"
#include "StagingDescriptorHeap.h"

#include "DepthPrepassNode.h"
#include "ShadowPassNode.h"
#include "PSSMNode.h"
#include "ForwardPassNode.h"
#include "SkyboxNode.h"
#include "BlurSSAONode.h"
#include "ToneMapNode.h"
#include "UIPassNode.h"
#include "HBAONode.h"

#include "Scene.h"

#include "ForwardCBProvider.h"

#include "ParallelSplitShadowMapping.h"


class SceneRenderer
{
public:

	enum class RenderMode
	{
		FullTonemapped
	};

	struct HBAOSettings
	{
		float max_r = 0.20f;
		float angle_bias = DirectX::XM_PIDIV2 / 10.0f;
		int nsamples_per_direction = 4;
	};

	struct TonemapSettings
	{
		float max_luminance = 9000;
		float min_luminance = 1.e-2f;
	};

	struct DeviceContext
	{
		ID3D12Device* device = nullptr;
		DescriptorTableBakery* srv_cbv_uav_tables = nullptr;
		GPUTaskQueue* graphics_queue = nullptr;
	};

	struct SceneContext
	{
		Scene* scene = nullptr;
		CameraID main_camera = CameraID::nullid;
		CameraID frustrum_cull_camera = CameraID::nullid; // if this is set to nullid, use main camera for frustrum culling

		// temporary
		DescriptorTableID ibl_table = DescriptorTableID::nullid;
	};

	struct Target
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtv;
		D3D12_VIEWPORT viewport;
		D3D12_RECT scissor_rect;
	};

	SceneRenderer( DeviceContext& ctx, uint32_t n_frames_in_flight );
	~SceneRenderer();

	SceneRenderer( const SceneRenderer& ) = delete;
	SceneRenderer& operator=( const SceneRenderer& ) = delete;
	SceneRenderer( SceneRenderer&& ) = default;
	SceneRenderer& operator=( SceneRenderer&& ) = default;

	// Target must be in D3D12_RESOURCE_STATE_RENDER_TARGET on graphics queue
	void Draw( const SceneContext& ctx, RenderMode mode, const Target& target );

	void SetTonemapSettings( const TonemapSettings& settings ) noexcept { m_tonemap_settings = settings; }
	TonemapSettings GetTonemapSettings() const noexcept { return m_tonemap_settings; }

	void SetHBAOSettings( const HBAOSettings& settings ) noexcept { m_hbao_settings = settings; }
	HBAOSettings GetHBAOSettings() const noexcept { return m_hbao_settings; }

	void SetPSSMUniformFactor( float factor ) noexcept { m_pssm.SetUniformFactor( factor ); }
	float GetPSSMUniformFactor() const noexcept { return m_pssm.GetUniformFactor(); }

	// Requires a complete rebuild of almost all transient resources and flushes the graphics queue
	void SetInternalResolution( uint32_t width, uint32_t height );
	std::pair<uint32_t, uint32_t> GetInternalResolution() const noexcept { return std::make_pair( m_resolution_width, m_resolution_height ); }

	DXGI_FORMAT GetTargetFormat( RenderMode mode ) const noexcept;
	// May involve PSO recompilation
	void SetTargetFormat( RenderMode mode, DXGI_FORMAT format );

	// Flushes previously set graphics queue
	void SetGraphicsQueue( GPUTaskQueue* queue );
	GPUTaskQueue* GetGraphicsQueue() const noexcept { return m_graphics_queue; }

private:
	// settings

	// internal resolution
	uint32_t m_resolution_width;
	uint32_t m_resolution_height;

	DXGI_FORMAT m_back_buffer_format;

	HBAOSettings m_hbao_settings;
	TonemapSettings m_tonemap_settings;
	ParallelSplitShadowMapping m_pssm;

	// framegraph
	using FramegraphInstance =
		Framegraph
		<
			DepthPrepassNode,
			ShadowPassNode,
			PSSMNode,
			ForwardPassNode,
			SkyboxNode,
			HBAONode,
			BlurSSAONode,
			ToneMapNode,
			UIPassNode
		>;
	FramegraphInstance m_framegraph;
	ForwardCBProvider m_forward_cb_provider;

	// transient resources
	ComPtr<ID3D12Resource> m_depth_stencil_buffer = nullptr;
	std::unique_ptr<DynamicTexture> m_fp_backbuffer = nullptr;
	std::unique_ptr<DynamicTexture> m_ambient_lighting = nullptr;
	std::unique_ptr<DynamicTexture> m_normals = nullptr;
	std::unique_ptr<DynamicTexture> m_ssao = nullptr;
	std::unique_ptr<DynamicTexture> m_ssao_blurred = nullptr;
	std::unique_ptr<DynamicTexture> m_ssao_blurred_transposed = nullptr;

	StagingDescriptorHeap m_dsv_heap;
	StagingDescriptorHeap m_rtv_heap;

	// permanent context
	ID3D12Device* m_device = nullptr;
	DescriptorTableBakery* m_descriptor_tables = nullptr;
	GPUTaskQueue* m_graphics_queue = nullptr;

	void InitTransientResourceDescriptors();
	void CreateTransientResources();
	void DestroyTransientResources();
};