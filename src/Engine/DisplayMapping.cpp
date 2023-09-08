#include "StdAfx.h"

#include "DisplayMapping.h"

#include "Rendergraph.h"
#include "Scene.h"
#include "ShaderPrograms.h"

#include <ImguiBackend/ImguiBackend.h>

void DisplayMapping::SetupRendergraph( SceneViewFrameData& data, DisplayMappingContext& ctx ) const
{
    const int cur_output_idx = data.scene_output_idx;
    ctx.input_tex = data.scene_output[cur_output_idx];

    if ( m_dbg_texture )
    {
        ctx.dbg_copy_pass = data.rg->AddPass( RHI::QueueType::Graphics, "DisplayMappingCopyDbgTexture" );
        ctx.dbg_copy_pass->UseTextureView( *ctx.input_tex->GetRTView() );
    }

    ctx.main_pass = data.rg->AddPass( RHI::QueueType::Graphics, "DisplayMapping" );
    ctx.main_pass->UseTextureView( *ctx.input_tex->GetROView() );
    const int new_output_idx = ( cur_output_idx + 1 ) % 2;

    data.scene_output_idx = new_output_idx;
    ctx.output_tex = data.scene_output[new_output_idx];
    ctx.main_pass->UseTextureView( *ctx.output_tex->GetRTView() );
}

void DisplayMapping::DisplayMappingPass( RHICommandList& cmd_list, const SceneViewFrameData& data, const DisplayMappingContext& ctx ) const
{
    BlitTextureProgram* blit_program = GetRenderer().GetBlitTextureProgram();

    if ( ctx.dbg_copy_pass )
    {
        ctx.dbg_copy_pass->BorrowCommandList( cmd_list );
        BlitTextureProgram::Params blit_parms = {};

        blit_parms.input = m_dbg_texture->GetTextureROView();
        blit_parms.output = ctx.input_tex->GetRTView()->GetRHIView();
        blit_parms.sampler = GetRenderer().GetPointSampler();

        blit_program->Run( cmd_list, *data.rg, blit_parms );
        ctx.dbg_copy_pass->EndPass();
    }

    {
        ctx.main_pass->BorrowCommandList( cmd_list );
        BlitTextureProgram::Params blit_parms = {};

        blit_parms.input = ctx.input_tex->GetROView()->GetRHIView();
        blit_parms.output = ctx.output_tex->GetRTView()->GetRHIView();
        blit_parms.sampler = GetRenderer().GetPointSampler();

        blit_program->Run( cmd_list, *data.rg, blit_parms );
        ctx.main_pass->EndPass();
    }
}

void DisplayMapping::DebugUI()
{
    ImGui::PushID( this );

    static std::string demo_texture_path;
    ImGui::InputText( "SourceTexture", &demo_texture_path );
    bool texture_path_changed = ImGui::IsItemDeactivatedAfterEdit();
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

    if ( texture_path_changed )
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