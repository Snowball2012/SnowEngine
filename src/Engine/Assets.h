#pragma once

#include <Core/Core.h>

class AssetId
{
	// todo: guid? hash?
	std::string m_path;

public:
	AssetId( const char* path ) :
		m_path( path )
	{}
};

// Asset state can only transition one way
// Loading --> Ready if loading was successful
//         \
//          -> Invalid if loading failed
// Hot reload is handled by creating a new Asset inside AssetManager
// Invalid asset can't become Ready and vice versa
enum class AssetStatus
{
	Loading = 0,
	Invalid,
	Ready
};

class Asset
{
	friend class AssetManager;
	class AssetManager* m_mgr = nullptr;

	AssetId m_id;

	AssetStatus m_status = AssetStatus::Loading;

	std::atomic<int> m_refs = 0;

	Asset( const AssetId& id, AssetManager& mgr )
		: m_mgr( &mgr ), m_id( id )
	{
	}

public:
	virtual ~Asset() {}

	void AddRef() { m_refs++; }
	void Release();

	virtual void Load() { m_status = AssetStatus::Invalid; }

	AssetStatus GetStatus() const { return m_status; }
};

inline void intrusive_ptr_add_ref( Asset* p )
{
	if ( p )
		p->AddRef();
}
inline void intrusive_ptr_release( Asset* p )
{
	if ( p )
		p->Release();
}

using AssetPtr = boost::intrusive_ptr<Asset>;

struct AssetManagerEntry
{
	Asset* asset = nullptr;
};

class AssetManager
{
	std::unordered_map<AssetId, AssetManagerEntry> m_assets;
	std::unordered_map<AssetId, AssetManagerEntry> m_orphans;

	std::mutex m_cs;
public:
	// user-facing
	AssetPtr Load( const AssetId& id );
	void UnloadAllOrphans();

	// asset-facing
	void Orphan( const AssetId& asset );
};
