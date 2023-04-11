#include "StdAfx.h"

#include "Assets.h"


void Asset::Release()
{
	if ( --m_refs <= 0 )
		m_mgr->Orphan( m_id );
}


AssetManager::AssetManager()
{
	RegisterGenerators();
}

AssetManager::~AssetManager()
{
	UnloadAllOrphans();

	if ( !m_assets.empty() )
	{
		SE_LOG_ERROR( Engine, "Found assets still in use when destroying AssetManager :" );
	}

	for ( auto& asset : m_assets )
	{
		SE_LOG_ERROR( Engine, "\t%s", asset.first.GetPath() );
		delete asset.second.asset;
	}
}

AssetPtr AssetManager::Load( const AssetId& id )
{
	std::scoped_lock lock( m_cs );

	// Find in caches
	auto entry = m_assets.find( id );
	
	Asset* found_asset = nullptr;
	
	if ( entry != m_assets.end() )
		found_asset = entry->second.asset;
	
	if ( !found_asset )
	{
		entry = m_orphans.find( id );
		if ( entry != m_orphans.end() )
		{
			m_assets[id] = entry->second;
			found_asset = entry->second.asset;
			m_orphans.erase( entry );
		}
	}
	
	if ( found_asset && found_asset->GetStatus() == AssetStatus::Invalid )
	{
		// try reloading? maybe track the number of reload attempts
		NOTIMPL;
	}

	if ( found_asset )
		return found_asset;

	// Load from disk
	// todo: memory-map the file and only read generator part to figure out asset class (should be the first)
	// Can probably skip even that if generator class is known beforehand
	std::string ospath = ToOSPath( id.GetPath() );
	std::ifstream file( ospath );
	if ( !file.good() )
	{
		SE_LOG_ERROR( Engine, "Can't open file %s (OS path: %s)", id.GetPath(), ospath );
		return nullptr;
	}

	Json d;
	rapidjson::IStreamWrapper isw( file );
	if ( d.ParseStream( isw ).HasParseError() )
	{
		SE_LOG_ERROR( Engine, "File %s (OS path: %s) is not a valid asset file : json parse error", id.GetPath(), ospath );
		return nullptr;
	}

	JsonValue::MemberIterator generator = d.FindMember( "_generator" );

	if ( generator == d.MemberEnd() || !generator->value.IsString() )
	{
		SE_LOG_ERROR( Engine, "File %s (OS path : %s) is not a valid asset file : _generator value is invalid", id.GetPath(), ospath );
		return nullptr;
	}

	auto factory = m_factories.find( generator->value.GetString() );
	if ( factory == m_factories.end() )
	{
		SE_LOG_ERROR( Engine, "File %s (OS path : %s) is not a valid asset file : _generator %s is not found", id.GetPath(), ospath, generator->value.GetString() );
		return nullptr;
	}

	Asset* created_asset = factory->second( id, *this );
	if ( !SE_ENSURE( created_asset ) )
		return nullptr;

	// todo: async loading
	if ( !created_asset->Load( d ) )
	{
		delete created_asset;
		return nullptr;
	}

	m_assets[id].asset = created_asset;

	return created_asset;
}

void AssetManager::UnloadAllOrphans()
{
	std::scoped_lock lock( m_cs );

	for ( auto& orphan : m_orphans )
	{
		delete orphan.second.asset;
	}

	SE_LOG_INFO( Engine, "Unloaded %u orphans", m_orphans.size() );
	
	m_orphans.clear();
}

void AssetManager::Orphan( const AssetId& asset_id )
{
	std::scoped_lock lock( m_cs );

	auto entry = m_assets.find( asset_id );
	if ( entry != m_assets.end() )
	{
		m_orphans[asset_id] = entry->second;
		m_assets.erase( entry );

		SE_LOG_INFO( Temp, "Orphan %s", asset_id.GetPath() );
	}
}

template <class AssetClass>
Asset* AssetManager::CreateAsset( const AssetId& id, AssetManager& mgr )
{
	return new AssetClass(id, mgr);
}


void AssetManager::RegisterGenerators()
{
	// todo: automatic generation?

#ifdef REGISTER_ASSET_GENERATOR
#error "Macro REGISTER_ASSET_GENERATOR is already defined"
#endif
#define REGISTER_ASSET_GENERATOR( className ) m_factories[ S_(className) ] = CreateAsset<className>;

	REGISTER_ASSET_GENERATOR( CubeAsset );

#undef REGISTER_ASSET_GENERATOR
}
