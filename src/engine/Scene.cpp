// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "Scene.h"

// StaticMesh

void StaticMesh::Load( const D3D12_VERTEX_BUFFER_VIEW& vbv, const D3D12_INDEX_BUFFER_VIEW& ibv ) noexcept
{
    m_vbv = vbv;
    m_ibv = ibv;
    m_is_loaded = true;
}

// Scene

// template helpers
template<> struct Scene::ID2Obj<TransformID>
{
    using type = ObjectTransform;
};

template<> struct Scene::ID2Obj<StaticMeshID>
{
    using type = StaticMesh;
};

template<> struct Scene::ID2Obj<StaticSubmeshID>
{
    using type = StaticSubmesh;
};

template<> struct Scene::ID2Obj<TextureID>
{
    using type = Texture;
};

template<> struct Scene::ID2Obj<CubemapID>
{
    using type = Cubemap;
};

template<> struct Scene::ID2Obj<MaterialID>
{
    using type = MaterialPBR;
};

template<> struct Scene::ID2Obj<EnvMapID>
{
    using type = EnvironmentMap;
};

template<> Scene::freelist_from_id<TransformID>& Scene::GetStorage<TransformID>() noexcept
{
    return m_obj_tfs;
}

template<> Scene::freelist_from_id<StaticMeshID>& Scene::GetStorage<StaticMeshID>() noexcept
{
    return m_static_meshes;
}

template<> Scene::freelist_from_id<StaticSubmeshID>& Scene::GetStorage<StaticSubmeshID>() noexcept
{
    return m_static_submeshes;
}

template<> Scene::freelist_from_id<TextureID>& Scene::GetStorage<TextureID>() noexcept
{
    return m_textures;
}

template<> Scene::freelist_from_id<CubemapID>& Scene::GetStorage<CubemapID>() noexcept
{
    return m_cubemaps;
}

template<> Scene::freelist_from_id<MaterialID>& Scene::GetStorage<MaterialID>() noexcept
{
    return m_materials;
}

template<> Scene::freelist_from_id<EnvMapID>& Scene::GetStorage<EnvMapID>() noexcept
{
    return m_env_maps;
}

// generic methods
template<typename IDType>
bool Scene::Remove( IDType obj_id ) noexcept
{
    auto* obj = GetStorage<IDType>().try_get( obj_id );
    if ( ! obj )
        return true;

    if ( obj->GetRefCount() != 0 )
        return false;

    GetStorage<IDType>().erase( obj_id );
    return true;
}


// Transforms

TransformID Scene::AddTransform( ) noexcept
{
    ObjectTransform tf;
    return m_obj_tfs.insert( std::move( tf ) );
}

bool Scene::RemoveTransform( TransformID id ) noexcept
{
    return Remove( id );
}

ObjectTransform* Scene::TryModifyTransform( TransformID id ) noexcept
{
    return m_obj_tfs.try_get( id );
}


// Static meshes

StaticMeshID Scene::AddStaticMesh() noexcept
{
    StaticMesh mesh;
    return m_static_meshes.insert( std::move( mesh ) );
}

bool Scene::RemoveStaticMesh( StaticMeshID id ) noexcept
{
    return Remove( id );
}

StaticMesh* Scene::TryModifyStaticMesh( StaticMeshID id ) noexcept
{
    return m_static_meshes.try_get( id );
}


// Submeshes
StaticSubmeshID Scene::AddStaticSubmesh( StaticMeshID mesh_id )
{
    StaticMesh* mesh = m_static_meshes.try_get( mesh_id );
    if ( ! mesh )
        throw SnowEngineException( "referenced mesh does not exist" );
    mesh->AddRef();

    StaticSubmesh submesh( mesh_id );
    return m_static_submeshes.insert( std::move( submesh ) );
}

bool Scene::RemoveStaticSubmesh( StaticSubmeshID id ) noexcept
{
    StaticSubmesh* submesh = m_static_submeshes.try_get( id );
    if ( ! submesh )
        return true;

    StaticMesh* mesh = m_static_meshes.try_get( submesh->GetMesh() );

    assert( mesh != nullptr ); // It's a strange situation indeed, but I don't see why we should fail here, since we are deleting this element anyway
    if ( mesh )
        mesh->ReleaseRef();

    return Remove( id );
}

StaticSubmesh* Scene::TryModifyStaticSubmesh( StaticSubmeshID id ) noexcept
{
    return m_static_submeshes.try_get( id );
}


// Textures

TextureID Scene::AddTexture() noexcept
{
    Texture texture;
    return m_textures.insert( std::move( texture ) );
}

bool Scene::RemoveTexture( TextureID id ) noexcept
{
    return Remove( id );
}

Texture* Scene::TryModifyTexture( TextureID id ) noexcept
{
    return m_textures.try_get( id );
}


// Cubemaps

CubemapID Scene::AddCubemap() noexcept
{
    Cubemap cubemap;
    return m_cubemaps.insert( std::move( cubemap ) );
}

bool Scene::RemoveCubemap( CubemapID id ) noexcept
{
    return Remove( id );
}

Cubemap* Scene::TryModifyCubemap( CubemapID id ) noexcept
{
    return m_cubemaps.try_get( id );
}


// Materials

MaterialID Scene::AddMaterial( const MaterialPBR::TextureIds& textures )
{
    auto add_ref = [&]( TextureID id )
    {
        Texture* tex = m_textures.try_get( id );
        if ( ! tex )
            throw SnowEngineException( "referenced texture does not exist" );
        tex->AddRef();
    };

    for ( const auto& tex_id : { textures.base_color, textures.normal, textures.specular, textures.preintegrated_brdf } )
        add_ref( tex_id );

    MaterialPBR material;
    material.Textures() = textures;
    return m_materials.insert( std::move( material ) );
}

bool Scene::RemoveMaterial( MaterialID id ) noexcept
{
    MaterialPBR* material = m_materials.try_get( id );
    if ( ! material )
        return true;

    auto release_ref = [&]( TextureID id )
    {
        Texture* tex = m_textures.try_get( id );
        assert( tex != nullptr );
        if ( tex )
            tex->ReleaseRef();
    };

    const auto& textures = material->Textures();
    for ( const auto& tex_id : { textures.base_color, textures.normal, textures.specular, textures.preintegrated_brdf } )
        release_ref( tex_id );

    return Remove( id );
}

MaterialPBR* Scene::TryModifyMaterial( MaterialID id ) noexcept
{
    return m_materials.try_get( id );
}


// Lights

LightID Scene::AddLight() noexcept
{
    return m_lights.emplace();
}

bool Scene::RemoveLight( LightID id ) noexcept
{
    bool has_light = m_lights.has( id );
    m_lights.erase( id );
    return has_light;
}

Light* Scene::TryModifyLight( LightID id ) noexcept
{
    return m_lights.try_get( id );
}

// Enviroment maps

EnvMapID Scene::AddEnviromentMap( CubemapID cubemap_id, TransformID tf_id )
{
    Cubemap* cubemap = m_cubemaps.try_get( cubemap_id );
    if ( ! cubemap )
        throw SnowEngineException( "referenced cubemap does not exist" );
    cubemap->AddRef();

    ObjectTransform* tf = m_obj_tfs.try_get( tf_id );
    if ( ! tf )
        throw SnowEngineException( "referenced transform does not exist" );
    tf->AddRef();

    EnvironmentMap envmap;

    envmap.Map() = cubemap_id;
    envmap.Transform() = tf_id;

    return m_env_maps.insert( std::move( envmap ) );
}

bool Scene::RemoveEnviromentMap( EnvMapID id ) noexcept
{
    EnvironmentMap* envmap = m_env_maps.try_get( id );
    if ( ! envmap )
        return true;

    Cubemap* cubemap = m_cubemaps.try_get( envmap->GetMap() );
    assert( cubemap != nullptr );
    if ( cubemap )
        cubemap->ReleaseRef();

    ObjectTransform* tf = m_obj_tfs.try_get( envmap->GetTransform() );
    assert( tf != nullptr );
    if ( tf )
        tf->ReleaseRef();

    return Remove( id );
}

EnvironmentMap* Scene::TryModifyEnvMap( EnvMapID id ) noexcept
{
    return m_env_maps.try_get( id );
}
