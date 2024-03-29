// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "ShadowProvider.h"

using namespace DirectX;

// temporary implementation, only one shadow map 4kx4k max

ShadowProvider::ShadowProvider( ID3D12Device* device, DescriptorTableBakery* srv_tables )
    : m_device( device ), m_dsv_heap( D3D12_DESCRIPTOR_HEAP_TYPE_DSV, device ), m_descriptor_tables( srv_tables )
{
    assert( device );
    assert( srv_tables );

    constexpr UINT width = ShadowMapSize;
    constexpr UINT height = ShadowMapSize;

    CD3DX12_RESOURCE_DESC tex_desc = CD3DX12_RESOURCE_DESC::Tex2D( ShadowMapResFormat,
                                                                   width, height,
                                                                   1, 1, 1, 0,
                                                                   D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL );
    D3D12_CLEAR_VALUE opt_clear;
    opt_clear.Format = ShadowMapDSVFormat;
    opt_clear.DepthStencil.Depth = 1.0f;
    opt_clear.DepthStencil.Stencil = 0;

    {
        ThrowIfFailedH( m_device->CreateCommittedResource( &CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT ), D3D12_HEAP_FLAG_NONE,
                                                          &tex_desc, D3D12_RESOURCE_STATE_COMMON,
                                                          &opt_clear, IID_PPV_ARGS( &m_sm_res ) ) );

        m_srv = m_descriptor_tables->AllocateTable( 1 );
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
        {
            srv_desc.Format = ShadowMapSRVFormat;
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Texture2D.MipLevels = 1;
        }
        m_device->CreateShaderResourceView( m_sm_res.Get(), &srv_desc, *m_descriptor_tables->ModifyTable( m_srv ) );

        m_dsv = std::make_unique<Descriptor>( std::move( m_dsv_heap.AllocateDescriptor() ) );
        D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
        {
            dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            dsv_desc.Format = ShadowMapDSVFormat;
            dsv_desc.Texture2D.MipSlice = 0;
            dsv_desc.Flags = D3D12_DSV_FLAG_NONE;
        }
        m_device->CreateDepthStencilView( m_sm_res.Get(), &dsv_desc, m_dsv->HandleCPU() );
    }

    {
        tex_desc.DepthOrArraySize = MAX_CASCADE_SIZE;
        tex_desc.Width = PSSMShadowMapSize;
        tex_desc.Height = PSSMShadowMapSize;

        ThrowIfFailedH( m_device->CreateCommittedResource( &CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT ), D3D12_HEAP_FLAG_NONE,
                                                          &tex_desc, D3D12_RESOURCE_STATE_COMMON,
                                                          &opt_clear, IID_PPV_ARGS( &m_pssm_res ) ) );

        m_pssm_srv = m_descriptor_tables->AllocateTable( 1 );
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
        {
            srv_desc.Format = ShadowMapSRVFormat;
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            srv_desc.Texture2DArray.MipLevels = 1;
            srv_desc.Texture2DArray.ArraySize = MAX_CASCADE_SIZE;
        }
        m_device->CreateShaderResourceView( m_pssm_res.Get(), &srv_desc, *m_descriptor_tables->ModifyTable( m_pssm_srv ) );

		for ( int i = 0; i < MAX_CASCADE_SIZE; ++i )
		{
			auto& descriptor = m_pssm_dsvs.emplace_back( std::move( m_dsv_heap.AllocateDescriptor() ) );
			D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
			{
				dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
				dsv_desc.Format = ShadowMapDSVFormat;
				dsv_desc.Texture2DArray.ArraySize = 1;
				dsv_desc.Texture2DArray.FirstArraySlice = i;
				dsv_desc.Flags = D3D12_DSV_FLAG_NONE;
			}
			m_device->CreateDepthStencilView( m_pssm_res.Get(), &dsv_desc, descriptor.HandleCPU() );
		}
    }
}

ShadowProvider::~ShadowProvider()
{
	m_dsv.reset();
	m_pssm_dsvs.clear();
}


std::vector<RenderTask::ShadowFrustum> ShadowProvider::Update( span<Light> scene_lights, const ParallelSplitShadowMapping& pssm, const Camera::Data& main_camera_data )
{
    std::vector<RenderTask::ShadowFrustum> frustums;

    std::array<float, MAX_CASCADE_SIZE - 1> split_positions_storage;

    auto split_positions = pssm.CalcSplitPositionsVS( main_camera_data, make_span( split_positions_storage ) );

    for ( Light& light : scene_lights )
    {
        if ( ! light.IsEnabled() )
            continue;

        if ( const auto& shadow_desc = light.GetShadow() )
        {
            const bool use_csm = shadow_desc->num_cascades > 1
                                 || light.GetData().type == Light::LightType::Parallel; // right now shaders require all parallel lights to use pssm

            if ( light.GetData().type != Light::LightType::Parallel && use_csm )
                NOTIMPL;

            if ( shadow_desc->num_cascades > MAX_CASCADE_SIZE )
                throw SnowEngineException( "too many cascades for a light" );

            auto& shadow_matrices = light.SetShadowMatrices();
            shadow_matrices.resize( shadow_desc->num_cascades );

            if ( use_csm )
                pssm.CalcShadowMatricesWS( main_camera_data, light, split_positions, make_span( shadow_matrices ) );

			RenderTask::ShadowFrustum shadow_frustum;
			shadow_frustum.split_positions.reserve( split_positions.size() );
			for ( const float pos : split_positions )
				shadow_frustum.split_positions.push_back( pos );

			for ( const auto& vp_matrix : shadow_matrices )
			{
				shadow_frustum.frustum.emplace_back( RenderTask::Frustum{
					RenderTask::Frustum::Type::Orthographic,
					DirectX::XMMatrixIdentity(),
					DirectX::XMMatrixIdentity(), vp_matrix } );
			}
			shadow_frustum.light = &light;
            frustums.push_back( shadow_frustum );
        }
    }

    return std::move( frustums );
}


void ShadowProvider::FillFramegraphStructures( const span<const LightInCB>& lights, const span<const RenderTask::ShadowRenderList>& renderlists, ShadowProducers& producers, ShadowCascadeProducers& pssm_producers, ShadowMaps& storage, ShadowCascade& pssm_storage )
{
    OPTICK_EVENT();
    CreateShadowProducers( lights );

	m_pssm_dsvs_for_framegraph.clear();
	for ( const auto& dsv_desc : m_pssm_dsvs )
		m_pssm_dsvs_for_framegraph.push_back( dsv_desc.HandleCPU() );

	for ( const auto& list : renderlists )
	{
		if ( list.cascade_renderlists.size() > 1 ) // cascade shadows
		{
			// find a producer
			auto light_it = std::find_if( lights.begin(), lights.end(), [&]( const auto& i ) { return i.light == list.light; } );
			if ( light_it != lights.end() )
			{
				const uint32_t producer_idx = std::distance( lights.begin(), light_it );
				auto& producer = m_pssm_producers[producer_idx];
				producer.casters.resize( list.cascade_renderlists.size() );
				for ( int i = 0; i < producer.casters.size(); ++i )
					for ( const auto& item : list.cascade_renderlists[i] )
						for ( const auto& batch : item)
							producer.casters[i].push_back( batch );
			}
			else
			{
				throw SnowEngineException( "found shadow items for unexpected light" );
			}
		}
		else
		{
			NOTIMPL; // repair regular non-cascade shadows
		}
	}

    producers.arr = make_span( m_producers );

    storage.res = m_sm_res.Get();
    storage.dsv = m_dsv->HandleCPU();
    storage.srv = m_descriptor_tables->GetTable( m_srv )->gpu_handle;

    pssm_producers.arr = make_span( m_pssm_producers );

    pssm_storage.res = m_pssm_res.Get();

    pssm_storage.dsvs = make_span( m_pssm_dsvs_for_framegraph );
    pssm_storage.srv = m_descriptor_tables->GetTable( m_pssm_srv )->gpu_handle;
}


void ShadowProvider::CreateShadowProducers( const span<const LightInCB>& lights )
{
    m_pssm_producers.clear();
    m_producers.clear();

    bool pssm_light_with_shadow_found = false;
    bool regular_light_with_shadow_found = false;
    for ( const LightInCB& light_in_cb : lights )
    {
        const Light& light = *light_in_cb.light;

        const auto& shadow_desc = light.GetShadow();
        if ( ! shadow_desc )
            continue;

        if ( light.GetShadowMatrices().size() > 1 || light.GetData().type == Light::LightType::Parallel )
        {
            if ( pssm_light_with_shadow_found )
                NOTIMPL; // pack all pssm shadow maps in one

            if ( light.GetData().type != Light::LightType::Parallel )
                NOTIMPL; // only parallel lights may be pssm

            pssm_light_with_shadow_found = true;

            m_pssm_producers.emplace_back();
            auto& producer = m_pssm_producers.back();
            if ( light.GetShadow()->sm_size > PSSMShadowMapSize )
                throw SnowEngineException( "pssm shadow map does not fit in texture" );

            // viewport
            producer.viewport.MaxDepth = 1.0f;
            producer.viewport.MinDepth = 0.0f;
            producer.viewport.TopLeftX = 0.0f;
            producer.viewport.TopLeftY = 0.0f;
            producer.viewport.Width = shadow_desc->sm_size;
            producer.viewport.Height = shadow_desc->sm_size;

            producer.light_idx_in_cb = light_in_cb.light_idx_in_cb;
        }
        else if ( light.GetShadowMatrices().size() == 1 )
        {
            NOTIMPL; // TODO: restore regular shadow mapping
        }
    }
}
