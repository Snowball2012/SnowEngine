#include "StdAfx.h"

#include "Assets.h"


void Asset::Release()
{
	if ( --m_refs <= 0 )
		m_mgr->Orphan( m_id );
}


AssetPtr AssetManager::Load( const AssetId& id )
{
	std::scoped_lock lock( m_cs );

	// Find in caches
	//auto entry = m_assets.find( id );
	//
	//Asset* found_asset = nullptr;
	//
	//if ( entry != m_assets.end() )
	//	found_asset = entry->second.asset;
	//
	//if ( !found_asset )
	//{
	//	entry = m_orphans.find( id );
	//	if ( entry != m_orphans.end() )
	//	{
	//		m_assets[id] = entry->second;
	//		found_asset = entry->second.asset;
	//		m_orphans.erase( entry );
	//	}
	//}
	//
	//if ( found_asset && found_asset->GetStatus() == AssetStatus::Invalid )
	//{
	//	// try reloading? maybe track the number of reload attempts
	//	NOTIMPL;
	//}

	// Load from disk
	NOTIMPL;

	return nullptr;
}

void AssetManager::UnloadAllOrphans()
{
	std::scoped_lock lock( m_cs );
	
	//for ( auto& orphan : m_orphans )
	//{
	//	delete orphan.second.asset;
	//}
	//
	//m_orphans.clear();
}

void AssetManager::Orphan( const AssetId& asset_id )
{
	std::scoped_lock lock( m_cs );

	//auto entry = m_assets.find( asset_id );
	//if ( entry != m_assets.end() )
	//{
	//	m_orphans[asset_id] = entry->second;
	//	m_assets.erase( entry );
	//}
}
