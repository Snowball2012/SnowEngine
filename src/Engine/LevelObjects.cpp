#include "StdAfx.h"

#include "LevelObjects.h"

#include "Assets.h"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

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
        if ( ImGui::TreeNode( trait.get(), "%s", trait->GetTraitPrettyName()) )
        {
            trait->OnUpdateGUI( trait_changed );
            ImGui::TreePop();
        }

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
    trait_changed = false;

    Transform tf = m_tf;
    ImGui::Checkbox( "Drag", &m_drag_translate );
    ImGui::SameLine();

    if ( m_drag_translate )
    {
        trait_changed |= ImGui::DragFloat3( "Translate", &tf.translation.x, 0.01f * std::max<float>( 0.1f, glm::length( tf.translation ) ) );
    }
    else
    {
        trait_changed |= ImGui::InputFloat3( "Translate", &tf.translation.x );
    }


    ImGui::Checkbox( "Drag##2", &m_drag_scale );
    ImGui::SameLine();
    if ( m_drag_scale )
    {
        trait_changed |= ImGui::DragFloat3( "Scale", &tf.scale.x, 0.01f * std::max<float>( 0.1f, glm::length( tf.scale ) ) );
    }
    else
    {
        trait_changed |= ImGui::InputFloat3( "Scale", &tf.scale.x );
    }

    if ( trait_changed )
    {
        m_tf = tf;
    }
}


// Mesh instance trait

bool MeshInstanceTrait::OnGenerate( WorldEntity base_entity, LevelObject& levelobj ) const
{
    MeshInstanceSetupComponent& component = levelobj.AddComponent<MeshInstanceSetupComponent>( base_entity );

    component.asset = m_mesh;

    if ( m_mesh != nullptr )
    {
        m_gui_path = m_mesh->GetPath();
    }

    return true;
}

void MeshInstanceTrait::OnUpdateGUI( bool& trait_changed )
{
    trait_changed = false;

    ImGui::InputText( "Asset", &m_gui_path );
    if ( ImGui::IsItemDeactivatedAfterEdit() )
    {
        if ( m_gui_path.empty() )
        {
            if ( m_mesh != nullptr )
            {
                trait_changed = true;
            }
        }
        else
        {
            MeshAssetPtr new_mesh = LoadAsset<MeshAsset>( m_gui_path.c_str() );
            if ( new_mesh.get() != m_mesh.get() )
            {
                m_mesh = new_mesh;
                trait_changed = true;
            }

            if ( new_mesh == nullptr )
            {
                SE_LOG_ERROR( Engine, "No mesh asset was found at path %s", m_gui_path.c_str() );
            }
        }
    }

    if ( trait_changed )
    {
        SE_LOG_INFO( Temp, "Mesh changed. New mesh path \"%s\"", m_mesh ? m_mesh->GetPath() : "null" );
    }
}

void MeshInstanceTrait::SetAsset( MeshAssetPtr mesh )
{
    m_mesh = mesh;
}
