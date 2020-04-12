#pragma once

#include "utils/span.h"

#include "StagingDescriptorHeap.h"
#include "DescriptorTableBakery.h"
#include "FramegraphResource.h"

#include "Scene.h"
#include "ParallelSplitShadowMapping.h"
#include "RenderTask.h"

// This class manages a storage for all shadow maps used in a frame.
// It packs all shadow maps into one texture and fills relevant framegraph structures

class ShadowProvider
{
public:
    ShadowProvider( ID3D12Device* device, DescriptorTableBakery* srv_tables );

    static std::vector<RenderTask::ShadowFrustum> Update( span<Light> scene_lights, const ParallelSplitShadowMapping& pssm, const Camera::Data& main_camera_data );

    void FillFramegraphStructures( const span<const LightInCB>& lights, const span<const RenderTask::ShadowRenderList>& renderlists,
                                   ShadowProducers& producers, ShadowCascadeProducers& pssm_producers,
                                   ShadowMaps& storage, ShadowCascade& pssm_storage );

private:
    using SrvID = DescriptorTableBakery::TableID;

    void CreateShadowProducers( const span<const LightInCB>& lights );
    
    std::vector<ShadowProducer> m_producers;
    std::unique_ptr<Descriptor> m_dsv = nullptr;
    SrvID m_srv = SrvID::nullid;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_sm_res;

    std::vector<ShadowCascadeProducer> m_pssm_producers;
    std::vector<Descriptor> m_pssm_dsvs;
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_pssm_dsvs_for_framegraph;
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