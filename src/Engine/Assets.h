#pragma once

#include "AssetManager.h"

#include <RHI/RHI.h>


struct MeshVertex;

struct MaterialGPU
{
	glm::vec3 albedo;
	glm::vec3 f0;
	float roughness;
};

class MaterialAsset : public Asset
{
	IMPLEMENT_ASSET_GENERATOR;

	uint32_t m_global_material_index = -1;

	MaterialGPU m_gpu_data = {};

public:
	virtual ~MaterialAsset() = default;

	MaterialAsset( const AssetId& id, AssetManager& mgr )
		: Asset( id, mgr )
	{}

	virtual bool Load( const JsonValue& data ) override;

	uint32_t GetGlobalMaterialIndex() const { return m_global_material_index; }

};
using MaterialAssetPtr = boost::intrusive_ptr<MaterialAsset>;


class MeshAsset : public Asset
{
	IMPLEMENT_ASSET_GENERATOR;

protected:

	RHIBufferPtr m_vertex_buffer = nullptr;
	RHIBufferPtr m_index_buffer = nullptr;
	RHIAccelerationStructurePtr m_blas = nullptr;

	uint32_t m_indices_num = 0;

	uint32_t m_global_geom_index = -1;

	MaterialAssetPtr m_default_material;

public:

	virtual bool Load( const JsonValue& data ) override;

	virtual ~MeshAsset() = default;

	RHIPrimitiveAttributeInfo GetPositionBufferInfo() const;
	size_t GetPositionBufferStride() const;

	const RHIBuffer* GetVertexBuffer() const { return m_vertex_buffer.get(); }
	const RHIBuffer* GetIndexBuffer() const { return m_index_buffer.get(); }
	const RHIAccelerationStructure* GetAccelerationStructure() const { return m_blas.get(); }

	const RHIIndexBufferType GetIndexBufferType() const;
	uint32_t GetNumIndices() const { return m_indices_num; }

	uint32_t GetGlobalGeomIndex() const { return m_global_geom_index; }

	const MaterialAsset* GetMaterial() const { return m_default_material.get(); }

protected:
	MeshAsset( const AssetId& id, AssetManager& mgr )
		: Asset( id, mgr )
	{}

	bool LoadFromObj( const char* path );

	bool LoadFromData( const std::span<MeshVertex>& vertices, const std::span<uint16_t>& indices );
};
using MeshAssetPtr = boost::intrusive_ptr<MeshAsset>;


class CubeAsset : public MeshAsset
{
	IMPLEMENT_ASSET_GENERATOR;

	static constexpr uint32_t m_cube_indices_count = 36;

public:

	virtual bool Load( const JsonValue& data ) override;

	virtual ~CubeAsset() = default;

protected:
	CubeAsset( const AssetId& id, AssetManager& mgr )
		: MeshAsset( id, mgr )
	{}
};
using CubeAssetPtr = boost::intrusive_ptr<CubeAsset>;


class TextureAsset : public Asset
{
	IMPLEMENT_ASSET_GENERATOR;

	RHITexturePtr m_rhi_texture = nullptr;
	RHITextureROViewPtr m_rhi_view = nullptr;

public:
	virtual ~TextureAsset() = default;

	TextureAsset( const AssetId& id, AssetManager& mgr )
		: Asset( id, mgr )
	{}

	virtual bool Load( const JsonValue& data ) override;

	bool LoadFromFile( const char* path );

	const RHITexture* GetTexture() const { return m_rhi_texture.get(); }
	RHITextureROView* GetTextureROView() const { return m_rhi_view.get(); }

private:
	bool LoadWithSTB( const char* ospath );
	bool LoadEXR( const char* ospath );
};
using TextureAssetPtr = boost::intrusive_ptr<TextureAsset>;
