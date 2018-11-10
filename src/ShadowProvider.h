#pragma once

#include "utils/span.h"

#include "StagingDescriptorHeap.h"
#include "DescriptorTableBakery.h"
#include "PipelineResource.h"

#include "Scene.h"

// This class manages a storage for all shadow map used in a frame.
// It packs all shadow maps into one texture and fills relevant pipeline structures

class ShadowProvider
{
public:
	ShadowProvider( ID3D12Device* device, int n_bufferized_frames, StagingDescriptorHeap* dsv_heap, DescriptorTableBakery* srv_tables, Scene* scene );

	void Update( span<SceneLight> scene_lights, const Camera::Data& main_camera_data );

	void FillPipelineStructures( span<const StaticMeshInstance> renderitems, ShadowProducers& producers, ShadowMapStorage& storage );

private:
	using SrvID = DescriptorTableBakery::TableID;

	void FillPassCB( const SceneLight& light,
					 const SceneLight::Shadow& shadow_desc,
					 const DirectX::XMFLOAT3& camera_pos,
					 PassConstants& gpu_data );

	void CalcLightingPassShadowMatrix( SceneLight& light, const DirectX::XMFLOAT4X4& pass_cb_matrix );

	std::vector<ShadowProducer> m_producers;
	std::unique_ptr<Descriptor> m_dsv = nullptr;
	SrvID m_srv = SrvID::nullid;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_sm_res;

	DescriptorTableBakery* m_descriptor_tables;
	Scene* m_scene;
	StagingDescriptorHeap* m_dsv_heap;
	ID3D12Device* m_device;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_pass_cb = nullptr;
	span<uint8_t> m_mapped_data;
	int m_cur_cb_idx = 0;
	const int m_nbuffers;

	static constexpr UINT ShadowMapSize = 4096;
	static constexpr DXGI_FORMAT ShadowMapResFormat = DXGI_FORMAT_R32_TYPELESS;
	static constexpr DXGI_FORMAT ShadowMapDSVFormat = DXGI_FORMAT_D32_FLOAT;
	static constexpr DXGI_FORMAT ShadowMapSRVFormat = DXGI_FORMAT_R32_FLOAT;
	static constexpr UINT BufferGPUSize = Utils::CalcConstantBufferByteSize( sizeof( PassConstants ) );
};