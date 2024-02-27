#include "StdAfx.h"

#include "LevelObjects.h"

#include "Assets.h"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

// Level object

std::unordered_map<std::string, LevelObject::TraitFactoryFunctionPtr> LevelObject::s_trait_factories = {};

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

bool LevelObject::Serialize( JsonValue& out, JsonAllocator& allocator ) const
{
    out.SetObject();

    JsonValue name_json_string;
    name_json_string.SetString( m_name.c_str(), allocator );
    out.AddMember( "name", name_json_string, allocator );

    JsonValue traits_arr_json;
    traits_arr_json.SetArray();
    traits_arr_json.Reserve( rapidjson::SizeType( m_traits.size() ), allocator );

    for ( const auto& trait : m_traits )
    {
        // @todo - proper trait reflection
        JsonValue trait_json;
        trait_json.SetObject();

        JsonValue trait_type_json_string;
        trait_type_json_string.SetString( trait->GetTraitClassName(), allocator );
        trait_json.AddMember( "_type", trait_type_json_string, allocator );

        JsonValue trait_contents;
        trait->Serialize( trait_contents, allocator );
        trait_json.AddMember( "value", trait_contents, allocator );

        traits_arr_json.PushBack( trait_json, allocator );
    }

    out.AddMember( "traits", traits_arr_json, allocator );

    return true;
}

bool LevelObject::Deserialize( const JsonValue& in, bool defer_regeneration )
{
    auto name_it = in.FindMember( "name" );
    if ( name_it == in.MemberEnd() || !name_it->value.IsString() )
    {
        SE_LOG_ERROR( Engine, "Level object does not have a name property" );
        return false;
    }

    SetName( name_it->value.GetString(), true );

    auto traits_it = in.FindMember( "traits" );
    if ( traits_it == in.MemberEnd() || !traits_it->value.IsArray() )
    {
        SE_LOG_ERROR( Engine, "Level object does not have a valid traits property" );
        return false;
    }

    auto traits_json_arr = traits_it->value.GetArray();
    int i = 0;
    for ( auto&& trait_json_value : traits_json_arr )
    {
        auto type_it = trait_json_value.FindMember( "_type" );
        if ( type_it == trait_json_value.MemberEnd() || !type_it->value.IsString() )
        {
            SE_LOG_ERROR( Engine, "Trait #%d does not have a valid _type property", i );
            return false;
        }

        auto value_it = trait_json_value.FindMember( "value" );
        if ( value_it == trait_json_value.MemberEnd() )
        {
            SE_LOG_ERROR( Engine, "Trait %s does not have a valid value property", type_it->value.GetString() );
            return false;
        }

        std::unique_ptr<LevelObjectTrait> trait = LevelObject::CreateTrait( type_it->value.GetString() );
        if ( !trait )
        {
            return false;
        }

        if ( !trait->Deserialize( value_it->value ) )
        {
            SE_LOG_ERROR( Engine, "Failed to deserialize trait %s", type_it->value.GetString() );
            return false;
        }

        m_traits.emplace_back( std::move( trait ) );

        i++;
    }

    if ( !defer_regeneration )
    {
        RegenerateEntities();
    }

    return true;
}

bool LevelObject::RegenerateEntities()
{
    DestroyEntities();
    return GenerateEntities();
}

bool LevelObject::SetPickingId( int32_t picking_id )
{
    for ( auto& entity : m_created_entities ) {
        m_world->AddComponent<EditorPickingComponent>( entity, EditorPickingComponent{ picking_id } );
    }

    m_picking_id = picking_id;

    return true;
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

    for ( auto& entity : m_created_entities ) {
        m_world->AddComponent<EditorPickingComponent>( entity, EditorPickingComponent{ m_picking_id } );
    }

    return true;
}

template<typename T>
std::unique_ptr<LevelObjectTrait> LevelObject::CreateTraitInternal()
{
    return std::make_unique<T>();
}

void LevelObject::RegisterTraits()
{
#ifdef REGISTER_TRAIT
#error "Macro REGISTER_TRAIT is already defined"
#endif
#define REGISTER_TRAIT( className ) s_trait_factories[ S_(className) ] = CreateTraitInternal<className>;

    REGISTER_TRAIT( TransformTrait );
    REGISTER_TRAIT( MeshInstanceTrait );

#undef REGISTER_ASSET_GENERATOR
}

std::unique_ptr<LevelObjectTrait> LevelObject::CreateTrait( const char* className )
{
    auto factory = s_trait_factories.find( className );

    if ( !SE_ENSURE( factory != s_trait_factories.end() ) )
        return nullptr; // if we are here, we probablyy forgot to register trait class inside RegisterTraits

    return factory->second();
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

    trait_changed |= ImGui::DragFloat3( "Translate", &tf.translation.x, 0.01f * std::max<float>( 0.1f, glm::length( tf.translation ) ) );

    {
        bool angles_changed = false;
        angles_changed = ImGui::DragFloat3( "Orientation (Pitch Yaw Roll)", &m_euler.x, 0.1f );
        
        if ( angles_changed )
        {
            trait_changed = true;

            m_euler.x = Wrap( m_euler.x, -180.0f, 180.0f );
            m_euler.y = Wrap( m_euler.y, -180.0f, 180.0f );
            m_euler.z = Wrap( m_euler.z, -180.0f, 180.0f );

            tf.orientation = glm::quat( glm::radians( m_euler ) );
        }
    }

    trait_changed |= ImGui::DragFloat3( "Scale", &tf.scale.x, 0.01f * std::max<float>( 0.1f, glm::length( tf.scale ) ) );

    if ( trait_changed )
    {
        m_tf = tf;
    }
}

bool TransformTrait::Serialize( JsonValue& out, JsonAllocator& allocator ) const
{
    Serialization::Serialize( m_tf, out, allocator );
    Serialization::Serialize( m_euler, out, allocator );

    return true;
}

bool TransformTrait::Deserialize( const JsonValue& in )
{
    bool has_tf = Serialization::Deserialize( in, m_tf );
    bool has_euler = Serialization::Deserialize( in, m_euler );
    if ( has_euler )
        m_tf.orientation = glm::quat( glm::radians( m_euler ) );

    return has_tf;
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
}

bool MeshInstanceTrait::Serialize( JsonValue& out, JsonAllocator& allocator ) const
{
    out.SetObject();

    std::string assetPath = "";
    if ( m_mesh != nullptr )
    {
        assetPath = m_mesh->GetPath();
    }

    JsonValue path_json_string;
    path_json_string.SetString( assetPath.c_str(), allocator );
    out.AddMember( "asset", path_json_string, allocator );

    return true;
}

bool MeshInstanceTrait::Deserialize( const JsonValue& in )
{
    if ( !in.IsObject() )
    {
        SE_LOG_ERROR( Engine, "%s: Json value is not an object", GetTraitPrettyName() );
        return false;
    }

    auto asset_it = in.FindMember( "asset" );
    if ( asset_it == in.MemberEnd() || !asset_it->value.IsString() )
    {
        SE_LOG_ERROR( Engine, "%s: asset property not found or invalid", GetTraitPrettyName() );
        return false;
    }

    m_mesh = LoadAsset<MeshAsset>( asset_it->value.GetString() );
    m_gui_path = m_mesh ? m_mesh->GetPath() : "";

    return true;
}

void MeshInstanceTrait::SetAsset( MeshAssetPtr mesh )
{
    m_mesh = mesh;
}
