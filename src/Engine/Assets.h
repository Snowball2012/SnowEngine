#pragma once

#include "AssetManager.h"

#include <RHI/RHI.h>

// Test asset
class CubeAsset : public Asset
{
	IMPLEMENT_ASSET_GENERATOR;

public:

	virtual bool Load( const JsonValue& data ) { m_status = AssetStatus::Ready; std::cout << "[Temp]: Cube loaded successfully" << std::endl; return true; }

	~CubeAsset() = default;

protected:
	CubeAsset( const AssetId& id, AssetManager& mgr )
		: Asset( id, mgr )
	{}
};