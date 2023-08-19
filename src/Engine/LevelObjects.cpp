#include "StdAfx.h"

#include "LevelObjects.h"

#include <imgui/imgui.h>

// Level object

LevelObject::LevelObject( World* world )
    : m_world( world )
{
    VERIFY_NOT_EQUAL( m_world, nullptr );
}

LevelObject::~LevelObject()
{
    DestroyEntities();
}

bool LevelObject::SetName( const char* name, bool defer_regeneration )
{
    m_name = name;
    if ( !defer_regeneration )
    {
        return RegenerateEntities();
    }
    return true;
}

const char* LevelObject::GetName() const
{
    return m_name.c_str();
}

World::Entity LevelObject::CreateChildEntity()
{
    NOTIMPL;
    return World::Entity();
}

bool LevelObject::AddTrait( std::unique_ptr<LevelObjectTrait>&& trait, bool defer_regeneration )
{
    m_traits.emplace_back( std::move( trait ) );

    if ( !defer_regeneration )
    {
        return RegenerateEntities();
    }

    return true;
}

bool LevelObject::OnUpdateGUI()
{
    bool needs_regeneration = false;
    for ( const auto& trait : m_traits )
    {
        bool trait_changed = false;
        if ( ImGui::BeginChild( trait->GetTraitPrettyName() ) )
        {
            trait->OnUpdateGUI( trait_changed );
        }
        ImGui::EndChild();

        if ( trait_changed )
        {
            needs_regeneration = true;
        }
    }
    return needs_regeneration;
}

bool LevelObject::RegenerateEntities()
{
    DestroyEntities();
    return GenerateEntities();
}

WorldEntity LevelObject::GetBaseEntity() const
{
    if ( m_created_entities.empty() )
        return WorldEntity::nullid;

    return m_created_entities.front();
}

void LevelObject::DestroyEntities()
{
    for ( WorldEntity entity : m_created_entities )
    {
        m_world->AddComponent<DestroyEntityComponent>( entity );
    }

    m_created_entities.clear();
}

bool LevelObject::GenerateEntities()
{
    WorldEntity& base_entity = m_created_entities.emplace_back();
    base_entity = m_world->CreateEntity();
    m_world->AddComponent<NameComponent>( base_entity ).name = m_name;

    for ( const auto& trait : m_traits )
    {
        if ( !trait->OnGenerate( base_entity, *this ) )
        {
            SE_LOG_ERROR( Engine, "Failed to regenerate entities from \"%s\" trait for object \"%s\", abort level obj generation", trait->GetTraitPrettyName(), m_name.c_str() );
            return false;
        }
    }

    return true;
}


// Transform trait

bool TransformTrait::OnGenerate( WorldEntity base_entity, LevelObject& levelobj ) const
{
    TransformComponent* existing_tf = levelobj.GetComponent<TransformComponent>( base_entity );
    if ( existing_tf != nullptr )
    {
        SE_LOG_ERROR( Engine, "Transform component already exists on entity when processing \"%s\" trait on object \"%s\"", GetTraitPrettyName(), levelobj.GetName() );
        return false;
    }

    TransformComponent& component = levelobj.AddComponent<TransformComponent>( base_entity );

    component.tf = m_tf;

    return true;
}

void TransformTrait::OnUpdateGUI( bool& trait_changed )
{
    float new_x_value = m_tf.translation.x;
    trait_changed = ImGui::SliderFloat( "Translate X", &new_x_value, -1, 1 );

    if ( trait_changed )
        m_tf.translation.x = new_x_value;
}


// Mesh instance trait

bool MeshInstanceTrait::OnGenerate( WorldEntity base_entity, LevelObject& levelobj ) const
{
    MeshInstanceSetupComponent& component = levelobj.AddComponent<MeshInstanceSetupComponent>( base_entity );

    component.asset = m_mesh;

    return true;
}

void MeshInstanceTrait::OnUpdateGUI( bool& trait_changed )
{
    trait_changed = false;
}

void MeshInstanceTrait::SetAsset( MeshAssetPtr mesh )
{
    m_mesh = mesh;
}
