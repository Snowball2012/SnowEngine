#pragma once

#include "Ptr.h"
#include "SceneSystemData.h"

#include "DescriptorTableBakery.h"
#include "StagingDescriptorHeap.h"

#include "CubemapConverter.h"

#include <dxtk12/ResourceUploadBatch.h>

// Static texture loader

class CubemapManager
{
public:
	CubemapManager( Microsoft::WRL::ComPtr<ID3D12Device> device, DescriptorTableBakery* descriptor_tables, Scene* scene ) noexcept;
	~CubemapManager();

	void CreateCubemapFromTexture( CubemapID cubemap_id, TextureID texture_id );

	void Update( );

	void OnBakeDescriptors( ID3D12GraphicsCommandList& cmd_list_graphics_queue );

private:
	struct CubemapData
	{
		CubemapID id;
		std::unique_ptr<Descriptor> staging_srv;
		Microsoft::WRL::ComPtr<ID3D12Resource> gpu_res;
	};

	struct ConvertationData
	{
		TextureID texture_id;
		DescriptorTableBakery::TableID texture_srv;
		CubemapData cubemap;
	};

	Microsoft::WRL::ComPtr<ID3D12Device> m_device;

	std::vector<CubemapData> m_loaded_cubemaps;

	std::vector<ConvertationData> m_conversion_in_progress;

	StagingDescriptorHeap m_srv_heap;
	DescriptorTableBakery* m_desc_tables;

	CubemapConverter m_converter;

	static constexpr uint32_t CubemapResolution = 2048;

	Scene* m_scene = nullptr;
};