#include "StdAfx.h"

#include "Editor.h"

#include <Engine/Scene.h>

#include <ImguiBackend/ImguiBackend.h>

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

        m_cam_angles_radians.x = fmodf( m_cam_angles_radians.x, M_PI * 2.0f );

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
