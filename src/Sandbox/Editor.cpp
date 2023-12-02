#include "StdAfx.h"

#include "Editor.h"

#include <Engine/Scene.h>

#include <ImguiBackend/ImguiBackend.h>


// EditorCamera

void EditorCamera::SetupView( SceneView& view ) const
{
    view.SetCameraOrientation( m_cam_orientation );
    view.SetCameraPosition( m_cam_pos );
    view.SetFOV( glm::radians( m_fov_degrees ) );

    if ( m_camera_moved )
    {
        view.ResetAccumulation();
        m_camera_moved = false;
    }
}

bool EditorCamera::UpdateGUI()
{
    ImGui::DragFloat( "Cam turning speed", &m_cam_turn_speed, std::max( 0.001f, m_cam_turn_speed ) );
    return ImGui::SliderFloat( "FoV (degrees)", &m_fov_degrees, 1.0f, 179.0f, "%.1f" );
}

void EditorCamera::Update( float delta_time )
{
    glm::vec3 camera_movement_ls = glm::vec3( 0, 0, 0 );
    if ( ImGui::GetIO().MouseDown[1] )
    {
        float cam_turn_speed = m_cam_turn_speed * ( m_fov_degrees / 45.0f );
        m_cam_angles_radians.x -= ImGui::GetIO().MouseDelta.x * cam_turn_speed;
        m_cam_angles_radians.y += ImGui::GetIO().MouseDelta.y * cam_turn_speed;
        m_cam_angles_radians.y = std::clamp<float>( m_cam_angles_radians.y, float( M_PI * 0.01f ), float( M_PI * 0.99f ) );

        m_cam_angles_radians.x = fmodf( m_cam_angles_radians.x, float( M_PI * 2.0f ) );

        if ( ImGui::IsKeyDown( ImGuiKey::ImGuiKey_W ) )
        {
            camera_movement_ls.z += 1.0f;
        }
        if ( ImGui::IsKeyDown( ImGuiKey::ImGuiKey_S ) )
        {
            camera_movement_ls.z -= 1.0f;
        }
        if ( ImGui::IsKeyDown( ImGuiKey::ImGuiKey_A ) )
        {
            camera_movement_ls.x -= 1.0f;
        }
        if ( ImGui::IsKeyDown( ImGuiKey::ImGuiKey_D ) )
        {
            camera_movement_ls.x += 1.0f;
        }
        if ( ImGui::IsKeyDown( ImGuiKey::ImGuiKey_C ) )
        {
            camera_movement_ls.y -= 1.0f;
        }
        if ( ImGui::IsKeyDown( ImGuiKey::ImGuiKey_Space ) )
        {
            camera_movement_ls.y += 1.0f;
        }
        camera_movement_ls *= m_cam_move_speed * delta_time;

        m_camera_moved = true;
    }

    m_cam_orientation = glm::quatLookAt( SphericalToCartesian( 1.0f, m_cam_angles_radians.x, m_cam_angles_radians.y ), glm::vec3( 0, 1, 0 ) );
    m_cam_pos += glm::rotate( m_cam_orientation, camera_movement_ls );
}


// LevelEditor

LevelEditor::LevelEditor()
{
    m_scene = std::make_unique<Scene>();
    m_scene_view = std::make_unique<SceneView>( m_scene.get() );

    m_world = std::make_unique<World>();
}

LevelEditor::~LevelEditor()
{
    m_level_objects.clear();
    m_scene_view = nullptr;
    m_scene = nullptr;
    m_world = nullptr;
}

bool LevelEditor::OpenLevel( const char* filepath )
{
    SE_LOG_INFO( Sandbox, "Open level %s", filepath );

    std::ifstream file( filepath );
    if ( !file.good() )
    {
        SE_LOG_ERROR( Engine, "Can't open level file %s", filepath );
        return false;
    }

    Json d;
    rapidjson::IStreamWrapper isw( file );
    if ( d.ParseStream( isw ).HasParseError() )
    {
        SE_LOG_ERROR( Engine, "File %s is not a valid asset file : json parse error", filepath );
        return false;
    }

    JsonValue::MemberIterator object_property = d.FindMember( "objects" );
    if ( object_property == d.MemberEnd() || !object_property->value.IsArray() )
    {
        SE_LOG_ERROR( Engine, "File %s is not a valid level file : objects value is invalid (not found or not an array)", filepath );
        return false;
    }

    m_level_objects.clear();

    for ( auto& v : object_property->value.GetArray() )
    {
        auto& new_object = m_level_objects.emplace_back( std::make_unique<LevelObject>( m_world.get() ) );
        if ( !new_object->Deserialize( v, false ) )
        {
            m_level_objects.pop_back();
            continue;
        }
    }

    return true;
}

bool LevelEditor::SaveLevel( const char* filepath ) const
{
    SE_LOG_INFO( Sandbox, "Save level to %s", filepath );

    std::ofstream file( filepath, std::ios_base::trunc );

    if ( !file.good() )
    {
        SE_LOG_ERROR( Sandbox, "Can't open file %s for writing", filepath );
        return false;
    }

    Json d;
    d.SetObject();

    JsonValue generator;
    generator.SetString( "LevelAsset", d.GetAllocator() );
    d.AddMember( "_generator", generator, d.GetAllocator() );
    JsonValue levelobjects_array;
    levelobjects_array.SetArray();
    levelobjects_array.Reserve( rapidjson::SizeType( m_level_objects.size() ), d.GetAllocator() );

    for ( const auto& level_obj : m_level_objects )
    {
        JsonValue obj_json;

        level_obj->Serialize( obj_json, d.GetAllocator() );
        levelobjects_array.PushBack( obj_json, d.GetAllocator() );
    }

    d.AddMember( "objects", levelobjects_array, d.GetAllocator() );

    rapidjson::OStreamWrapper osw( file );
    rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer( osw );
    d.Accept( writer );

    return true;
}

bool LevelEditor::Update( float delta_time_sec )
{
    // Level outliner
    bool scene_view_changed = false;
    ImGui::Begin( "Level Editor" );
    {
        scene_view_changed |= m_editor_camera.UpdateGUI();

        {
            ImGui::Text( "Total: %llu level objects", m_level_objects.size() );
            bool create_object = ImGui::Button( "Add object" );
            ImGui::SameLine();
            static std::string new_object_name;
            ImGui::InputText( "Name", &new_object_name );

            ImGui::Text( "Level Objects" );
            if ( ImGui::BeginListBox( "##levelobjectslistbox", ImVec2( -FLT_MIN, 5 * ImGui::GetTextLineHeightWithSpacing() ) ) )
            {
                for ( int n = 0; n < m_level_objects.size(); n++ )
                {
                    const bool is_selected = ( m_selected_object == n );
                    if ( ImGui::Selectable( m_level_objects[n]->GetName(), is_selected) )
                        m_selected_object = n;

                    // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                    if ( is_selected )
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndListBox();
            }

            if ( create_object )
            {
                auto& new_object = m_level_objects.emplace_back( std::make_unique<LevelObject>( m_world.get() ) );
                new_object->SetName( new_object_name.c_str(), true );
                new_object->RegenerateEntities();
                scene_view_changed = true;

                m_selected_object = int( m_level_objects.size() ) - 1;
            }

            if ( m_selected_object >= 0 && m_selected_object < int( m_level_objects.size() ) )
            {
                auto& level_object = m_level_objects[m_selected_object];

                ImGui::PushID( level_object.get() );
                ImGui::SeparatorText( level_object->GetName() );
                {
                    bool create_trait = ImGui::Button( "Add" );
                    ImGui::SameLine();
                    static int item_current = 0;
                    ImGui::Combo( "Trait", &item_current, "Transform\0MeshInstance\0" );

                    if ( create_trait )
                    {
                        switch ( item_current )
                        {
                        case 0:
                        {
                            level_object->AddTrait( std::make_unique<TransformTrait>(), true );
                        }
                        break;
                        case 1:
                        {
                            level_object->AddTrait( std::make_unique<MeshInstanceTrait>(), true );
                        }
                        break;
                        }
                        level_object->RegenerateEntities();
                        scene_view_changed = true;
                    }

                    if ( level_object->OnUpdateGUI() )
                    {
                        level_object->RegenerateEntities();
                        scene_view_changed = true;
                    }
                }
                ImGui::PopID();
            }
        }
    }

    if ( scene_view_changed && m_scene_view != nullptr )
    {
        m_scene_view->ResetAccumulation();
    }
    ImGui::End();

    UpdateCamera( delta_time_sec );

    // handle deselection
    if ( ImGui::IsKeyPressed( ImGuiKey_Escape, false ) )
    {
        m_selected_object = -1;
    }

    // Update world
    WorldUtils::DestroyMarkedEntities( *m_world, m_scene.get() );
    WorldUtils::SetupEntities( *m_world, m_scene.get() );

    UpdateScene();

    return true;
}

bool LevelEditor::Draw( Rendergraph& framegraph, ISceneRenderExtension* required_extension )
{
    m_scene->Synchronize();

    RenderSceneParams parms = {};
    parms.rg = &framegraph;
    parms.view = m_scene_view.get();
    parms.extension = required_extension;

    GetRenderer().RenderScene(parms);

    return true;
}

bool LevelEditor::SetViewportExtents( glm::uvec2 extents )
{
    m_scene_view->SetExtents( extents );
    return false;
}

void LevelEditor::ResetAccumulation()
{
    m_scene_view->ResetAccumulation();
}

void LevelEditor::UpdateCamera( float delta_time_sec )
{
    m_editor_camera.Update( delta_time_sec );
    m_editor_camera.SetupView( *m_scene_view );
}

void LevelEditor::UpdateScene()
{
    // this should be somewhere in the engine code

    // @todo - run only for dirty transforms
    for ( const auto& [entity_id, tf, mesh_instance_component] : m_world->CreateView<TransformComponent, MeshInstanceComponent>() )
    {
        SceneMeshInstance* smi = m_scene->GetMeshInstance( mesh_instance_component.scene_mesh_instance );
        if ( !SE_ENSURE( smi ) )
            continue;

        smi->m_tf = tf.tf;
    }

    ImVec2 imgui_mouse_pos = ImGui::GetMousePos();
    glm::uvec2 scene_cursor_pos = glm::uvec2( imgui_mouse_pos.x, imgui_mouse_pos.y );
    m_scene_view->SetCursorPosition( scene_cursor_pos );
}
