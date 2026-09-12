#include "StdAfx.h"

#include "MaterialEditor.h"

#include "Editor.h"

#include <Engine/EngineApp.h>

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <ImGuiFileDialog/ImGuiFileDialog.h>

namespace
{
    constexpr const char* MATERIAL_OPEN_DIALOG = "MaterialOpenDlg";
    constexpr const char* MATERIAL_NEW_DIALOG = "MaterialNewDlg";
    constexpr const char* TEXTURE_DIALOGS[] =
    {
        "MaterialAlbedoTextureDlg",
        "MaterialSpecularTextureDlg",
        "MaterialNormalTextureDlg",
        "MaterialEmissiveTextureDlg"
    };

    void SetTexturePath( MaterialDescription& description, size_t index, const std::string& path )
    {
        switch ( index )
        {
        case 0: description.albedo_texture = path; break;
        case 1: description.specular_texture = path; break;
        case 2: description.normal_texture = path; break;
        case 3: description.emissive_texture = path; break;
        }
    }

    bool NormalizeEnginePath( const std::string& input, std::string& asset_path, std::string& os_path )
    {
        std::error_code error;
        const std::filesystem::path root = std::filesystem::weakly_canonical(
            std::filesystem::path( g_core_paths.engine_content ), error );
        if ( error )
            return false;

        std::filesystem::path candidate = input.rfind( "#engine/", 0 ) == 0
            ? std::filesystem::path( ToOSPath( input.c_str() ) )
            : std::filesystem::path( input );
        candidate = std::filesystem::weakly_canonical( candidate, error );
        if ( error )
            return false;

        const std::filesystem::path relative = std::filesystem::relative( candidate, root, error );
        if ( error || relative.empty() || relative == "." || relative.is_absolute() )
            return false;

        for ( const std::filesystem::path& component : relative )
        {
            if ( component == ".." )
                return false;
        }

        asset_path = "#engine/" + relative.generic_string();
        os_path = candidate.generic_string();
        return true;
    }

    void AddStringMember(
        Json& document,
        JsonValue& object,
        const char* name,
        const std::string& value )
    {
        JsonValue json_name;
        json_name.SetString( name, document.GetAllocator() );
        JsonValue json_value;
        json_value.SetString( value.c_str(), rapidjson::SizeType( value.size() ), document.GetAllocator() );
        object.AddMember( json_name, json_value, document.GetAllocator() );
    }

    bool WriteMaterialFile(
        const std::string& os_path,
        const MaterialDescription& description,
        std::string& error_message )
    {
        std::ofstream file( os_path, std::ios::trunc );
        if ( !file )
        {
            error_message = "Could not open material for writing: " + os_path;
            return false;
        }

        Json document;
        document.SetObject();
        JsonValue generator;
        generator.SetString( "MaterialAsset", document.GetAllocator() );
        document.AddMember( "_generator", generator, document.GetAllocator() );

        JsonValue parameters( rapidjson::kObjectType );
        JsonValue albedo;
        Serialization::Serialize( description.albedo, albedo, document.GetAllocator() );
        parameters.AddMember( "albedo", albedo, document.GetAllocator() );
        JsonValue f0;
        Serialization::Serialize( description.f0, f0, document.GetAllocator() );
        parameters.AddMember( "f0", f0, document.GetAllocator() );
        parameters.AddMember( "roughness", description.roughness, document.GetAllocator() );
        JsonValue emissive;
        Serialization::Serialize( description.emissive, emissive, document.GetAllocator() );
        parameters.AddMember( "emissive", emissive, document.GetAllocator() );
        AddStringMember( document, parameters, "albedo_texture", description.albedo_texture );
        AddStringMember( document, parameters, "specular_texture", description.specular_texture );
        AddStringMember( document, parameters, "normal_texture", description.normal_texture );
        AddStringMember( document, parameters, "emissive_texture", description.emissive_texture );
        document.AddMember( "parms", parameters, document.GetAllocator() );

        rapidjson::OStreamWrapper stream( file );
        rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer( stream );
        if ( !document.Accept( writer ) )
        {
            error_message = "Could not serialize material: " + os_path;
            return false;
        }
        file << '\n';
        file.close();
        if ( !file )
        {
            error_message = "Could not finish writing material: " + os_path;
            return false;
        }
        return true;
    }

    bool VerifyMaterialFile( const std::string& os_path )
    {
        std::ifstream file( os_path );
        if ( !file )
            return false;

        Json document;
        rapidjson::IStreamWrapper stream( file );
        if ( document.ParseStream( stream ).HasParseError() )
            return false;

        const auto generator = document.FindMember( "_generator" );
        const auto parameters = document.FindMember( "parms" );
        return generator != document.MemberEnd() && generator->value.IsString() &&
            strcmp( generator->value.GetString(), "MaterialAsset" ) == 0 &&
            parameters != document.MemberEnd() && parameters->value.IsObject();
    }
}

void MaterialEditor::Show()
{
    m_visible = true;
}

void MaterialEditor::OpenMaterial( MaterialAssetPtr material )
{
    if ( material == nullptr )
        return;
    if ( material.get() == m_material.get() )
    {
        m_visible = true;
        return;
    }
    RequestAction( PendingAction::OpenMaterial, std::move( material ) );
}

void MaterialEditor::SetMaterial( MaterialAssetPtr material )
{
    m_material = std::move( material );
    m_edit = m_material->GetDescription();
    m_saved = m_edit;
    m_texture_paths =
    {
        m_edit.albedo_texture,
        m_edit.specular_texture,
        m_edit.normal_texture,
        m_edit.emissive_texture
    };
    m_dirty = false;
    m_error.clear();
    m_visible = true;
}

void MaterialEditor::QueueUpdate( const MaterialAssetPtr& material )
{
    const auto found = std::find_if(
        m_pending.begin(),
        m_pending.end(),
        [&]( const MaterialAssetPtr& queued ) { return queued.get() == material.get(); } );
    if ( found == m_pending.end() )
        m_pending.push_back( material );
}

bool MaterialEditor::ApplyEdit()
{
    if ( m_material == nullptr )
        return false;
    if ( !m_material->ApplyDescription( m_edit ) )
    {
        m_error = "Material values could not be applied; the last valid values remain active";
        SE_LOG_ERROR( Sandbox, "%s", m_error.c_str() );
        m_edit = m_material->GetDescription();
        return false;
    }

    QueueUpdate( m_material );
    m_dirty = true;
    m_error.clear();
    return true;
}

bool MaterialEditor::CommitTexture( size_t texture_index )
{
    std::string canonical_path;
    std::string os_path;
    if ( !m_texture_paths[texture_index].empty() &&
        !NormalizeEnginePath( m_texture_paths[texture_index], canonical_path, os_path ) )
    {
        m_error = "Texture must be an asset inside EngineContent";
        return false;
    }

    MaterialDescription candidate = m_edit;
    SetTexturePath( candidate, texture_index, canonical_path );
    if ( !m_material->ApplyDescription( candidate ) )
    {
        m_error = "Texture could not be loaded; the previous texture remains active";
        SE_LOG_ERROR( Sandbox, "%s: %s", m_error.c_str(), m_texture_paths[texture_index].c_str() );
        return false;
    }

    m_edit = std::move( candidate );
    m_texture_paths[texture_index] = canonical_path;
    QueueUpdate( m_material );
    m_dirty = true;
    m_error.clear();
    return true;
}

bool MaterialEditor::Save()
{
    if ( m_material == nullptr )
    {
        m_error = "No material is open";
        return false;
    }

    const std::string os_path = ToOSPath( m_material->GetPath() );
    if ( !WriteMaterialFile( os_path, m_edit, m_error ) )
    {
        SE_LOG_ERROR( Sandbox, "%s", m_error.c_str() );
        return false;
    }

    m_saved = m_edit;
    m_dirty = false;
    m_error.clear();
    SE_LOG_INFO( Sandbox, "Saved material %s", m_material->GetPath() );
    return true;
}

bool MaterialEditor::LoadFile( const std::string& path )
{
    std::string asset_path;
    std::string os_path;
    if ( !NormalizeEnginePath( path, asset_path, os_path ) )
    {
        m_error = "Material must be inside EngineContent";
        return false;
    }

    MaterialAssetPtr material = LoadAsset<MaterialAsset>( asset_path.c_str() );
    if ( material == nullptr )
    {
        m_error = "Could not load material: " + asset_path;
        SE_LOG_ERROR( Sandbox, "%s", m_error.c_str() );
        return false;
    }
    SetMaterial( std::move( material ) );
    return true;
}

bool MaterialEditor::CreateFile( const std::string& requested_path )
{
    std::filesystem::path path( requested_path );
    std::string extension = path.extension().string();
    std::transform( extension.begin(), extension.end(), extension.begin(), ::tolower );
    if ( extension != ".sea" )
        path += ".sea";

    std::string asset_path;
    std::string os_path;
    if ( !NormalizeEnginePath( path.generic_string(), asset_path, os_path ) )
    {
        m_error = "New material must be inside EngineContent";
        return false;
    }

    std::error_code filesystem_error;
    if ( std::filesystem::exists( os_path, filesystem_error ) )
    {
        m_error = "A material already exists at: " + os_path;
        return false;
    }
    if ( filesystem_error )
    {
        m_error = "Could not inspect material path: " + filesystem_error.message();
        return false;
    }

    const MaterialDescription defaults;
    if ( !WriteMaterialFile( os_path, defaults, m_error ) )
        return false;
    if ( !VerifyMaterialFile( os_path ) )
    {
        m_error = "New material file could not be verified: " + os_path;
        SE_LOG_ERROR( Sandbox, "%s", m_error.c_str() );
        return false;
    }
    return LoadFile( asset_path );
}

void MaterialEditor::RequestAction( PendingAction action, MaterialAssetPtr material )
{
    m_pending_action = action;
    m_pending_material = std::move( material );
    if ( m_dirty )
    {
        m_show_unsaved_popup = true;
        m_visible = true;
        return;
    }
    ExecutePendingAction();
}

void MaterialEditor::ExecutePendingAction()
{
    const PendingAction action = m_pending_action;
    m_pending_action = PendingAction::None;

    switch ( action )
    {
    case PendingAction::New:
        ImGuiFileDialog::Instance()->OpenDialog(
            MATERIAL_NEW_DIALOG,
            "New Material",
            ".sea",
            ( g_core_paths.engine_content + "/Materials/" ).c_str() );
        break;
    case PendingAction::Open:
        ImGuiFileDialog::Instance()->OpenDialog(
            MATERIAL_OPEN_DIALOG,
            "Open Material",
            ".sea",
            ( g_core_paths.engine_content + "/Materials/" ).c_str() );
        break;
    case PendingAction::Close:
        m_visible = false;
        break;
    case PendingAction::OpenMaterial:
        if ( m_pending_material != nullptr )
            SetMaterial( std::move( m_pending_material ) );
        break;
    case PendingAction::None:
        break;
    }
    m_pending_material = nullptr;
}

void MaterialEditor::DrawUnsavedPopup()
{
    if ( m_show_unsaved_popup )
    {
        ImGui::OpenPopup( "Unsaved Material Changes" );
        m_show_unsaved_popup = false;
    }

    if ( !ImGui::BeginPopupModal( "Unsaved Material Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) )
        return;

    ImGui::TextUnformatted( "Save changes to the current material?" );
    if ( ImGui::Button( "Save" ) )
    {
        if ( Save() )
        {
            ImGui::CloseCurrentPopup();
            ExecutePendingAction();
        }
    }
    ImGui::SameLine();
    if ( ImGui::Button( "Discard" ) )
    {
        if ( m_material->ApplyDescription( m_saved ) )
        {
            QueueUpdate( m_material );
            m_edit = m_saved;
            m_dirty = false;
            ImGui::CloseCurrentPopup();
            ExecutePendingAction();
        }
        else
        {
            m_error = "Could not restore the saved material values";
        }
    }
    ImGui::SameLine();
    if ( ImGui::Button( "Cancel" ) )
    {
        m_pending_action = PendingAction::None;
        m_pending_material = nullptr;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void MaterialEditor::OnUpdate( LevelEditor& editor )
{
    if ( m_visible )
    {
        bool window_open = true;
        const bool draw_contents = ImGui::Begin( "Material Editor", &window_open );
        if ( draw_contents )
        {
            if ( ImGui::Button( "New" ) )
                RequestAction( PendingAction::New );
            ImGui::SameLine();
            if ( ImGui::Button( "Open" ) )
                RequestAction( PendingAction::Open );
            ImGui::SameLine();
            if ( ImGui::Button( "Save" ) )
                Save();

            if ( m_material == nullptr )
            {
                ImGui::TextUnformatted( "No material selected" );
            }
            else
            {
                ImGui::SeparatorText( "Material" );
                ImGui::Text( "%s%s", m_material->GetPath(), m_dirty ? " *" : "" );

                bool changed = ImGui::ColorEdit3( "Albedo", &m_edit.albedo.x );
                changed |= ImGui::ColorEdit3( "F0", &m_edit.f0.x );
                changed |= ImGui::DragFloat( "Roughness", &m_edit.roughness, 0.01f, 0.02f, 1.0f );
                changed |= ImGui::DragFloat3( "Emissive", &m_edit.emissive.x, 0.01f, 0.0f, 100.0f );
                if ( changed )
                    ApplyEdit();

                ImGui::SeparatorText( "Textures" );
                const char* labels[] =
                {
                    "Albedo Texture",
                    "Specular / Roughness Texture",
                    "Normal Texture",
                    "Emissive Texture"
                };
                for ( size_t index = 0; index < std::size( labels ); ++index )
                {
                    ImGui::PushID( int( index ) );
                    ImGui::SetNextItemWidth( -120.0f );
                    ImGui::InputText( labels[index], &m_texture_paths[index] );
                    if ( ImGui::IsItemDeactivatedAfterEdit() )
                        CommitTexture( index );
                    ImGui::SameLine();
                    if ( ImGui::Button( "Browse" ) )
                    {
                        ImGuiFileDialog::Instance()->OpenDialog(
                            TEXTURE_DIALOGS[index],
                            labels[index],
                            ".sea",
                            ( g_core_paths.engine_content + "/Textures/" ).c_str() );
                    }
                    ImGui::SameLine();
                    if ( ImGui::Button( "Clear" ) )
                    {
                        m_texture_paths[index].clear();
                        CommitTexture( index );
                    }
                    ImGui::PopID();
                }
            }

            if ( !m_error.empty() )
                ImGui::TextColored( ImVec4( 1.0f, 0.3f, 0.3f, 1.0f ), "%s", m_error.c_str() );
        }
        ImGui::End();

        if ( !window_open )
            RequestAction( PendingAction::Close );
    }

    DrawUnsavedPopup();

    if ( ImGuiFileDialog::Instance()->Display( MATERIAL_OPEN_DIALOG ) )
    {
        if ( ImGuiFileDialog::Instance()->IsOk() )
            LoadFile( ImGuiFileDialog::Instance()->GetFilePathName() );
        ImGuiFileDialog::Instance()->Close();
    }
    if ( ImGuiFileDialog::Instance()->Display( MATERIAL_NEW_DIALOG ) )
    {
        if ( ImGuiFileDialog::Instance()->IsOk() )
            CreateFile( ImGuiFileDialog::Instance()->GetFilePathName() );
        ImGuiFileDialog::Instance()->Close();
    }
    for ( size_t index = 0; index < std::size( TEXTURE_DIALOGS ); ++index )
    {
        if ( ImGuiFileDialog::Instance()->Display( TEXTURE_DIALOGS[index] ) )
        {
            if ( ImGuiFileDialog::Instance()->IsOk() )
            {
                m_texture_paths[index] = ImGuiFileDialog::Instance()->GetFilePathName();
                CommitTexture( index );
            }
            ImGuiFileDialog::Instance()->Close();
        }
    }

    if ( !m_pending.empty() )
    {
        GetRHI().WaitIdle();
        bool any_updated = false;
        for ( const MaterialAssetPtr& material : m_pending )
        {
            const bool updated = GetRenderer().GetGlobalDescriptors().UpdateMaterial(
                material->GetGlobalMaterialIndex(), material->GetGPUData() );
            any_updated |= updated;
            if ( !updated )
                SE_LOG_ERROR( Sandbox, "Could not upload material %s", material->GetPath() );
        }
        m_pending.clear();
        if ( any_updated )
            editor.ResetAccumulation();
    }
}
