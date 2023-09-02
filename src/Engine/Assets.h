#pragma once

#include "AssetManager.h"

#include <RHI/RHI.h>


struct MeshVertex;


class MeshAsset : public Asset
{
	IMPLEMENT_ASSET_GENERATOR;

protected:

	RHIBufferPtr m_vertex_buffer = nullptr;
	RHIBufferPtr m_index_buffer = nullptr;
	RHIAccelerationStructurePtr m_blas = nullptr;

	uint32_t m_indices_num = 0;

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

public:
	virtual ~TextureAsset() = default;

	TextureAsset( const AssetId& id, AssetManager& mgr )
		: Asset( id, mgr )
	{}

	virtual bool Load( const JsonValue& data ) override;

	bool LoadFromFile( const char* path );

	const RHITexture* GetTexture() const { return m_rhi_texture.get(); }
};
using TextureAssetPtr = boost::intrusive_ptr<TextureAsset>;