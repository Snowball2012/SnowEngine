#pragma once

#include "AssetManager.h"

#include <RHI/RHI.h>

// Test asset
class CubeAsset : public Asset
{
	IMPLEMENT_ASSET_GENERATOR;

	RHIBufferPtr m_vertex_buffer = nullptr;
	RHIBufferPtr m_index_buffer = nullptr;
	RHIAccelerationStructurePtr m_blas = nullptr;

	static constexpr uint32_t m_indices_count = 36;

public:

	virtual bool Load( const JsonValue& data );

	~CubeAsset() = default;

	RHIPrimitiveAttributeInfo GetPositionBufferInfo() const;
	size_t GetPositionBufferStride() const;

	const RHIBuffer* GetVertexBuffer() const { return m_vertex_buffer.get(); }
	const RHIBuffer* GetIndexBuffer() const { return m_index_buffer.get(); }
	const RHIAccelerationStructure* GetAccelerationStructure() const { return m_blas.get(); }

	const RHIIndexBufferType GetIndexBufferType() const;
	uint32_t GetNumIndices() const { return m_indices_count; }

protected:
	CubeAsset( const AssetId& id, AssetManager& mgr )
		: Asset( id, mgr )
	{}
};
using CubeAssetPtr = boost::intrusive_ptr<CubeAsset>;