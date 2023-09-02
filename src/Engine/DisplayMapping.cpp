#include "StdAfx.h"

#include "DisplayMapping.h"
#include <ImguiBackend/ImguiBackend.h>

void DisplayMapping::DebugUI()
{
    ImGui::PushID( this );

    static std::string demo_texture_path;
    bool texture_path_changed = false;
    ImGui::InputText( "SourceTexture", &demo_texture_path );
    ImGui::SameLine();
    if ( ImGui::Button( "..." ) )
    {
        std::string default_path = g_core_paths.engine_content + "/Textures/";
        ImGuiFileDialog::Instance()->OpenDialog( "DisplayMappingDbgOpenDemoTexture", "Open Texture", ".sea", default_path.c_str() );
    }

    if ( ImGuiFileDialog::Instance()->Display( "DisplayMappingDbgOpenDemoTexture" ) )
    {
        if ( ImGuiFileDialog::Instance()->IsOk() )
        {
            demo_texture_path = ImGuiFileDialog::Instance()->GetFilePathName();
            texture_path_changed = true;
        }

        ImGuiFileDialog::Instance()->Close();
    }

    if ( ImGui::IsItemDeactivatedAfterEdit() || texture_path_changed )
    {
        if ( demo_texture_path.empty() )
        {
            m_dbg_texture = nullptr;
        }
        else
        {
            TextureAssetPtr new_texture = LoadAsset<TextureAsset>( demo_texture_path.c_str() );
            if ( new_texture != nullptr )
            {
                m_dbg_texture = new_texture;
            }

            if ( new_texture == nullptr )
            {
                SE_LOG_ERROR( Engine, "No texture asset was found at path %s", demo_texture_path.c_str() );
            }
        }
    }

    ImGui::PopID();
}