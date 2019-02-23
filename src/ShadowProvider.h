#pragma once

#include "utils/span.h"

#include "StagingDescriptorHeap.h"
#include "DescriptorTableBakery.h"
#include "FramegraphResource.h"

#include "Scene.h"
#include "ParallelSplitShadowMapping.h"

// This class manages a storage for all shadow maps used in a frame.
// It packs all shadow maps into one texture and fills relevant framegraph structures

class ShadowProvider
{
public:
	ShadowProvider( ID3D12Device* device, int n_bufferized_frames, DescriptorTableBakery* srv_tables );

	void Update( span<SceneLight> scene_lights, const ParallelSplitShadowMapping& pssm, const Camera::Data& main_camera_data );

	void FillFramegraphStructures( const Scene& scene, const span<const LightInCB>& lights, const span<const StaticMeshInstance>& renderitems,
	                               ShadowProducers& producers, ShadowCascadeProducers& pssm_producers,
								   ShadowMaps& storage, ShadowCascade& pssm_storage );

private:
	using SrvID = DescriptorTableBakery::TableID;

	void CreateShadowProducers( const span<const LightInCB>& lights );
	void FillProducersWithRenderitems( const span<const StaticMeshInstance>& renderitems, const Scene& scene );

	std::vector<ShadowProducer> m_producers;
	std::unique_ptr<Descriptor> m_dsv = nullptr;
	SrvID m_srv = SrvID::nullid;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_sm_res;

	std::vector<ShadowCascadeProducer> m_pssm_producers;
	std::unique_ptr<Descriptor> m_pssm_dsv = nullptr;
	SrvID m_pssm_srv = SrvID::nullid;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_pssm_res;

	StagingDescriptorHeap m_dsv_heap;

	DescriptorTableBakery* m_descriptor_tables;
	ID3D12Device* m_device;

	static constexpr UINT ShadowMapSize = 2048;
	static constexpr UINT PSSMShadowMapSize = 2048;
	static constexpr DXGI_FORMAT ShadowMapResFormat = DXGI_FORMAT_R32_TYPELESS;
	static constexpr DXGI_FORMAT ShadowMapDSVFormat = DXGI_FORMAT_D32_FLOAT;
	static constexpr DXGI_FORMAT ShadowMapSRVFormat = DXGI_FORMAT_R32_FLOAT;
};