#pragma once

#include "StdAfx.h"

class AssetId
{
	// todo: guid? hash?
	std::string m_path;

	friend struct std::hash<AssetId>;

public:
	AssetId( const char* path ) :
		m_path( path )
	{}

	const char* GetPath() const { return m_path.c_str(); }

	bool operator==( const AssetId& rhs ) const = default;
};

template<>
struct std::hash<AssetId>
{
	size_t operator()( const AssetId& v ) const { return std::hash<std::string>{}( v.m_path ); }
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
	std::atomic<int> m_refs = 0;

protected:
	AssetId m_id;

	AssetStatus m_status = AssetStatus::Loading;

public:
	virtual ~Asset() {}

	void AddRef() { m_refs++; }
	void Release();

	virtual bool Load( const JsonValue& data ) { m_status = AssetStatus::Invalid; return false; }

	AssetStatus GetStatus() const { return m_status; }

	const char* GetPath() const { return m_id.GetPath(); }

protected:
	Asset( const AssetId& id, AssetManager& mgr )
		: m_mgr( &mgr ), m_id( id )
	{
	}
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

	using AssetFactoryFunctionPtr = Asset * ( * )( const AssetId&, AssetManager& );
	std::unordered_map<std::string, AssetFactoryFunctionPtr> m_factories;

	std::mutex m_cs;

public:
	AssetManager();
	~AssetManager();

	// user-facing
	AssetPtr Load( const AssetId& id );
	void UnloadAllOrphans();

	// asset-facing
	void Orphan( const AssetId& asset );

private:
	template <class AssetClass>
	static Asset* CreateAsset( const AssetId& id, AssetManager& mgr );

	void RegisterGenerators();
};

#define IMPLEMENT_ASSET_GENERATOR private: friend class AssetManager


// helper

template < typename AssetClass >
boost::intrusive_ptr<AssetClass> LoadAsset( const char* name )
{
	return boost::dynamic_pointer_cast< AssetClass >( GetAssetManager().Load( AssetId( name ) ) );
}