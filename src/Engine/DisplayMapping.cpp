#include "StdAfx.h"

#include "DisplayMapping.h"

#include "Rendergraph.h"
#include "Scene.h"
#include "ShaderPrograms.h"

#include <ImguiBackend/ImguiBackend.h>

CVAR_DEFINE( r_showMagnifier, uint32_t, 0, "Show magnifier to inspect pixel-level details" );

// @todo - unify code with other screen-space programs, the code is identical
class DisplayMappingProgram : public ShaderProgram
{
private:
    RHIDescriptorSetLayoutPtr m_dsl = nullptr;

    RHIShaderPtr m_vs = nullptr;
    RHIShaderPtr m_ps = nullptr;

    mutable std::unordered_map<RHIFormat, RHIGraphicsPipelinePtr> m_psos;

public:
    // has to be in sync with DisplayMappingParams in hlsl
    struct DisplayMappingParams
    {
        uint32_t method;
        uint32_t show_hue_test_image;
        float white_point;
        uint32_t apply_magnifier;
    };

    struct Params
    {
        RHIRenderTargetView* output = nullptr;
        RHITextureROView* input = nullptr;
        RHISampler* sampler = nullptr;

        DisplayMapping::Method method = DisplayMapping::Method::Linear;
        float white_point = 1.0f;
        bool dbg_show_hue_test_image = false;

        RHIDescriptorSet* view_ds = nullptr;
    };    

    DisplayMappingProgram( RHIDescriptorSetLayout * view_dsl )
        : ShaderProgram()
    {
        m_type = ShaderProgramType::Raster;

        {
            RHI::DescriptorViewRange ranges[3] = {};

            ranges[0].type = RHIShaderBindingType::TextureRO;
            ranges[0].count = 1;
            ranges[0].stages = RHIShaderStageFlags::PixelShader;

            ranges[1].type = RHIShaderBindingType::Sampler;
            ranges[1].count = 1;
            ranges[1].stages = RHIShaderStageFlags::PixelShader;

            ranges[2].type = RHIShaderBindingType::UniformBuffer;
            ranges[2].count = 1;
            ranges[2].stages = RHIShaderStageFlags::PixelShader;

            RHI::DescriptorSetLayoutInfo binding_table = {};
            binding_table.ranges = ranges;
            binding_table.range_count = std::size( ranges );

            m_dsl = GetRHI().CreateDescriptorSetLayout( binding_table );

            RHI::ShaderBindingLayoutInfo layout_info = {};
            RHIDescriptorSetLayout* dsls[2] = { m_dsl.get(), view_dsl };
            layout_info.tables = dsls;
            layout_info.table_count = std::size( dsls );

            m_layout = GetRHI().CreateShaderBindingLayout( layout_info );
        }

        {
            RHI::ShaderCreateInfo create_info = {};
            std::wstring shader_path = ToWString( ToOSPath( "#engine/shaders/DisplayMapping.hlsl" ).c_str() );
            create_info.filename = shader_path.c_str();

            create_info.frequency = RHI::ShaderFrequency::Vertex;
            create_info.entry_point = "DisplayMappingVS";
            m_vs = GetRHI().CreateShader( create_info );

            create_info.frequency = RHI::ShaderFrequency::Pixel;
            create_info.entry_point = "DisplayMappingPS";
            m_ps = GetRHI().CreateShader( create_info );
        }
    }

    bool Run( RHICommandList& cmd_list, Rendergraph& rg, const Params& parms ) const
    {
        if ( !SE_ENSURE( parms.output && parms.input ) )
            return false;

        const RHIGraphicsPipeline* pso = GetPSO( parms.output->GetFormat() );

        if ( !SE_ENSURE( pso ) )
            return false;

        RHIPassRTVInfo rt = {};
        rt.load_op = RHILoadOp::Clear;
        rt.store_op = RHIStoreOp::Store;
        rt.rtv = parms.output;
        rt.clear_value.float32[3] = 1.0f;

        RHIPassInfo pass_info = {};
        glm::uvec2 output_extent = glm::uvec2( parms.output->GetSize() );
        pass_info.render_area = RHIRect2D{ .offset = glm::ivec2( 0,0 ), .extent = output_extent };
        pass_info.render_targets = &rt;
        pass_info.render_targets_count = 1;
        cmd_list.BeginPass( pass_info );

        cmd_list.SetPSO( *pso );

        RHIDescriptorSet* pass_descset = rg.AllocateFrameDescSet( *m_dsl );

        DisplayMappingParams pass_params_ub = {};
        pass_params_ub.method = uint32_t( parms.method );
        pass_params_ub.white_point = parms.white_point;
        pass_params_ub.show_hue_test_image = parms.dbg_show_hue_test_image ? 1 : 0;
        pass_params_ub.apply_magnifier = r_showMagnifier.GetValue();

        UploadBufferRange pass_ub = rg.AllocateUploadBufferUniform<DisplayMappingParams>();
        pass_ub.UploadData( pass_params_ub );

        RHIViewport viewport = {};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = float( output_extent.x );
        viewport.height = float( output_extent.y );
        viewport.min_depth = 0.0f;
        viewport.max_depth = 1.0f;

        RHIRect2D scissor = {};
        scissor.offset = { 0, 0 };
        scissor.extent = output_extent;

        cmd_list.SetViewports( 0, &viewport, 1 );
        cmd_list.SetScissors( 0, &scissor, 1 );

        pass_descset->BindTextureROView( 0, 0, *parms.input );
        pass_descset->BindSampler( 1, 0, *parms.sampler );
        pass_descset->BindUniformBufferView( 2, 0, pass_ub.view );
        cmd_list.BindDescriptorSet( 0, *pass_descset );

        cmd_list.BindDescriptorSet( 1, *parms.view_ds );

        cmd_list.Draw( 3, 1, 0, 0 );

        cmd_list.EndPass();

        return true;
    }

private:

    const RHIGraphicsPipeline* GetPSO( RHIFormat output_format ) const
    {
        auto& pso = m_psos[output_format];

        if ( pso == nullptr )
        {
            RHIInputAssemblerInfo ia_info = {};

            RHIGraphicsPipelineInfo rhi_pipeline_info = {};

            rhi_pipeline_info.input_assembler = &ia_info;
            rhi_pipeline_info.vs = m_vs.get();
            rhi_pipeline_info.ps = m_ps.get();

            RHIPipelineRTInfo rt_info = {};
            rt_info.format = output_format;
            rhi_pipeline_info.rts_count = 1;
            rhi_pipeline_info.rt_info = &rt_info;
            rhi_pipeline_info.binding_layout = m_layout.get();

            rhi_pipeline_info.rasterizer.cull_mode = RHICullModeFlags::None;

            pso = GetRHI().CreatePSO( rhi_pipeline_info );
        }

        return pso.get();
    }
};

DisplayMapping::DisplayMapping( RHIDescriptorSetLayout* view_dsl )
{
    m_program = std::make_unique<DisplayMappingProgram>( view_dsl );
}

DisplayMapping::~DisplayMapping() = default;

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
        DisplayMappingProgram::Params prog_parms = {};

        prog_parms.input = ctx.input_tex->GetROView()->GetRHIView();
        prog_parms.output = ctx.output_tex->GetRTView()->GetRHIView();
        prog_parms.sampler = GetRenderer().GetPointSampler();
        prog_parms.method = m_method;
        prog_parms.white_point = m_white_point;
        prog_parms.dbg_show_hue_test_image = m_dbg_show_hue_test_image;
        prog_parms.view_ds = data.view_desc_set;

        m_program->Run( cmd_list, *data.rg, prog_parms );
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

    {
        const char* items[] =
        {
            "Linear",
            "Reinhard",
            "Reinhard - with white point",
            "Reinhard - luminance only",
            "Reinhard - Jodie",
            "Uncharted 2",
            "ACES fitted",
            "ACEScg",
            "AgX"
        };
        static int item_current = int( m_method );
        ImGui::ListBox( "Method", &item_current, items, int( std::size( items ) ), 4 );
        m_method = Method( item_current );
        ImGui::Checkbox( "Show hue test image", &m_dbg_show_hue_test_image );
    }

    ImGui::DragFloat( "WhitePoint", &m_white_point, 0.1f * m_white_point );

    if ( m_white_point < 1.e-6f )
    {
        m_white_point = 1.e-6f;
    }

    ImGui::PopID();
}