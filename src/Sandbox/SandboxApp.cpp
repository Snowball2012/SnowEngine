#include "StdAfx.h"

#include "SandboxApp.h"

#include <Engine/Assets.h>
#include <Engine/RHIUtils.h>

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

CorePaths g_core_paths;
Logger* g_log;
EngineGlobals g_engine;

SandboxApp::SandboxApp()
    : EngineApp()
{
}

SandboxApp::~SandboxApp() = default;

void SandboxApp::OnInit()
{
    SE_LOG_INFO( Sandbox, "Sandbox initialization started" );

    m_cube = boost::dynamic_pointer_cast< CubeAsset >( m_asset_mgr->Load( AssetId( "#engine/Meshes/Cube.sea" ) ) );
    SE_ENSURE( m_cube && m_cube->GetStatus() == AssetStatus::Ready );

    CreateDescriptorSetLayout();
    CreatePipeline();
    CreateCubePipeline();
    CreateTextureImage();
    CreateTextureImageView();
    CreateTextureSampler();
    CreateVertexBuffer();
    CreateIndexBuffer();
    CreateUniformBuffers();
    CreateDescriptorSets();

    m_cube_instance_tlas_id = m_tlas.Instances().emplace();
    m_tlas.Instances()[m_cube_instance_tlas_id].blas = m_cube->GetAccelerationStructure();

    SE_LOG_INFO( Sandbox, "Sandbox initialization complete" );
}

void SandboxApp::OnCleanup()
{
    SE_LOG_INFO( Sandbox, "Sandbox shutdown started" );

    m_binding_tables.clear();
    m_uniform_buffer_views.clear();
    m_uniform_buffers.clear();
    m_index_buffer = nullptr;
    m_vertex_buffer = nullptr;

    m_texture_sampler = nullptr;
    m_texture_srv = nullptr;
    m_texture = nullptr;

    CleanupPipeline();

    m_vertex_buffer = nullptr;
    m_index_buffer = nullptr;
    m_uniform_buffers.clear();
    m_texture = nullptr;

    m_tlas.Reset();

    m_cube = nullptr;

    SE_LOG_INFO( Sandbox, "Sandbox shutdown complete" );
}

void SandboxApp::CleanupPipeline()
{
    m_rhi_graphics_pipeline = nullptr;
    m_binding_table_layout = nullptr;
    m_shader_bindings_layout = nullptr;
    m_cube_graphics_pipeline = nullptr;
}

void SandboxApp::CreateIndexBuffer()
{
    const std::vector<uint16_t> indices =
    {
        0, 1, 2, 2, 3, 0
    };

    RHI::BufferInfo buf_info = {};
    buf_info.size = sizeof( indices[0] ) * indices.size();;
    buf_info.usage = RHIBufferUsageFlags::IndexBuffer;
    m_index_buffer = RHIUtils::CreateInitializedGPUBuffer( buf_info, indices.data(), buf_info.size );
}

void SandboxApp::CreateDescriptorSetLayout()
{
    RHI::DescriptorViewRange ranges[3] = {};
    ranges[0].type = RHIShaderBindingType::ConstantBuffer;
    ranges[0].count = 1;
    ranges[0].stages = RHIShaderStageFlags::VertexShader;

    ranges[1].type = RHIShaderBindingType::TextureSRV;
    ranges[1].count = 1;
    ranges[1].stages = RHIShaderStageFlags::PixelShader;

    ranges[2].type = RHIShaderBindingType::Sampler;
    ranges[2].count = 1;
    ranges[2].stages = RHIShaderStageFlags::PixelShader;

    RHI::DescriptorSetLayoutInfo binding_table = {};
    binding_table.ranges = ranges;
    binding_table.range_count = std::size( ranges );

    m_binding_table_layout = m_rhi->CreateDescriptorSetLayout( binding_table );

    RHI::ShaderBindingLayoutInfo layout_info = {};
    RHIDescriptorSetLayout* dsls[1] = { m_binding_table_layout.get() };
    layout_info.tables = dsls;
    layout_info.table_count = std::size( dsls );

    m_shader_bindings_layout = m_rhi->CreateShaderBindingLayout( layout_info );
}

void SandboxApp::CreateUniformBuffers()
{
    m_uniform_buffers.resize( m_max_frames_in_flight );
    m_uniform_buffer_views.resize( m_max_frames_in_flight );

    RHI::BufferInfo uniform_info = {};
    uniform_info.size = sizeof( Matrices );
    uniform_info.usage = RHIBufferUsageFlags::UniformBuffer;

    for ( size_t i = 0; i < m_max_frames_in_flight; ++i )
    {
        m_uniform_buffers[i] = m_rhi->CreateUploadBuffer( uniform_info );
        RHI::CBVInfo view_info = {};
        view_info.buffer = m_uniform_buffers[i]->GetBuffer();
        m_uniform_buffer_views[i] = m_rhi->CreateCBV( view_info );
    }
}

void SandboxApp::UpdateUniformBuffer( uint32_t current_image )
{
    static auto start_time = std::chrono::high_resolution_clock::now();

    auto current_time = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>( current_time - start_time ).count();

    Matrices matrices = {};
    matrices.model = glm::rotate( glm::mat4( 1.0f ), time * glm::radians( 90.0f ), glm::vec3( 0, 1, 0 ) );

    m_tlas.Instances()[m_cube_instance_tlas_id].transform = glm::mat3x4( matrices.model[0], matrices.model[1], matrices.model[2] );

    matrices.view = glm::lookAt( glm::vec3( 2, 2, 2 ), glm::vec3( 0, 0, 0 ), glm::vec3( 0, 1, 0 ) );

    glm::uvec2 swapchain_extent = m_swapchain->GetExtent();
    matrices.proj = glm::perspective( glm::radians( 45.0f ), float( swapchain_extent.x ) / float( swapchain_extent.y ), 0.1f, 10.0f );
    matrices.proj[1][1] *= -1; // ogl -> vulkan y axis

    m_uniform_buffers[current_image]->WriteBytes( &matrices, sizeof( matrices ) );
}

void SandboxApp::CopyBufferToImage( RHIBuffer & src, RHITexture & texture, uint32_t width, uint32_t height )
{
    RHICommandList* list = RHIUtils::BeginSingleTimeCommands( RHI::QueueType::Graphics );

    RHIBufferTextureCopyRegion region = {};
    region.texture_subresource.mip_count = 1;
    region.texture_subresource.array_count = 1;
    region.texture_extent[0] = width;
    region.texture_extent[1] = height;
    region.texture_extent[2] = 1;

    list->CopyBufferToTexture( src, texture, &region, 1 );

    RHIUtils::EndSingleTimeCommands( *list );
}

void SandboxApp::CreateDescriptorSets()
{
    m_binding_tables.resize( m_max_frames_in_flight );

    for ( size_t i = 0; i < m_max_frames_in_flight; ++i )
    {
        m_binding_tables[i] = m_rhi->CreateDescriptorSet( *m_binding_table_layout );
        m_binding_tables[i]->BindCBV( 0, 0, *m_uniform_buffer_views[i] );
        m_binding_tables[i]->BindSRV( 1, 0, *m_texture_srv );
        m_binding_tables[i]->BindSampler( 2, 0, *m_texture_sampler );
        m_binding_tables[i]->FlushBinds(); // optional, will be flushed anyway on cmdlist.BindDescriptorSet
    }
}

void SandboxApp::CreateTextureImage()
{
    // upload to staging
    int width, height, channels;
    stbi_uc* pixels = stbi_load( ToOSPath( "#Engine/Textures/DemoStatue.jpg" ).c_str(), &width, &height, &channels, STBI_rgb_alpha);

    VERIFY_NOT_EQUAL( pixels, nullptr );

    uint64_t image_size = uint32_t( width ) * uint32_t( height ) * 4;

    RHI::BufferInfo staging_info = {};

    staging_info.size = image_size;
    staging_info.usage = RHIBufferUsageFlags::TransferSrc;

    RHIUploadBufferPtr staging_buf = m_rhi->CreateUploadBuffer( staging_info );

    VERIFY_NOT_EQUAL( staging_buf, nullptr );

    staging_buf->WriteBytes( pixels, staging_info.size, 0 );

    stbi_image_free( pixels );

    // image creation
    CreateImage(
        uint32_t( width ), uint32_t( height ),
        RHIFormat::R8G8B8A8_SRGB,
        RHITextureUsageFlags::SRV | RHITextureUsageFlags::TransferDst,
        RHITextureLayout::TransferDst,
        m_texture );

    CopyBufferToImage( *staging_buf->GetBuffer(), *m_texture, uint32_t( width ), uint32_t( height ) );

    TransitionImageLayoutAndFlush(
        *m_texture,
        RHITextureLayout::TransferDst,
        RHITextureLayout::ShaderReadOnly );
}

void SandboxApp::CreateImage(
    uint32_t width, uint32_t height, RHIFormat format, RHITextureUsageFlags usage, RHITextureLayout layout,
    RHITexturePtr & image )
{
    RHI::TextureInfo tex_info = {};
    tex_info.dimensions = RHITextureDimensions::T2D;
    tex_info.width = width;
    tex_info.height = height;
    tex_info.depth = 1;
    tex_info.mips = 1;
    tex_info.array_layers = 1;
    tex_info.format = format;
    tex_info.usage = usage;
    tex_info.initial_layout = layout;

    image = m_rhi->CreateTexture( tex_info );
}

void SandboxApp::TransitionImageLayoutAndFlush( RHITexture & texture, RHITextureLayout old_layout, RHITextureLayout new_layout )
{
    RHICommandList* cmd_list = RHIUtils::BeginSingleTimeCommands( RHI::QueueType::Graphics );

    TransitionImageLayout( *cmd_list, texture, old_layout, new_layout );

    RHIUtils::EndSingleTimeCommands( *cmd_list );
}

void SandboxApp::TransitionImageLayout( RHICommandList & cmd_list, RHITexture & texture, RHITextureLayout old_layout, RHITextureLayout new_layout )
{
    RHITextureBarrier barrier = {};
    barrier.layout_src = old_layout;
    barrier.layout_dst = new_layout;
    barrier.texture = &texture;
    barrier.subresources.mip_count = 1;
    barrier.subresources.array_count = 1;

    cmd_list.TextureBarriers( &barrier, 1 );
}

void SandboxApp::CreateTextureImageView()
{
    RHI::TextureSRVInfo srv_info = {};
    srv_info.texture = m_texture.get();
    m_texture_srv = m_rhi->CreateSRV( srv_info );
}

void SandboxApp::CreateTextureSampler()
{
    RHI::SamplerInfo info = {};
    info.enable_anisotropy = true;
    m_texture_sampler = m_rhi->CreateSampler( info );
}

void SandboxApp::CreateCubePipeline()
{
    RHI::ShaderCreateInfo create_info = {};
    std::wstring shader_path = ToWString( ToOSPath( "#engine/shaders/DemoCube.hlsl" ).c_str() );
    create_info.filename = shader_path.c_str();

    create_info.frequency = RHI::ShaderFrequency::Vertex;
    create_info.entry_point = "TriangleVS";
    RHIObjectPtr<RHIShader> triangle_shader_vs = m_rhi->CreateShader( create_info );

    create_info.frequency = RHI::ShaderFrequency::Pixel;
    create_info.entry_point = "TrianglePS";
    RHIObjectPtr<RHIShader> triangle_shader_ps = m_rhi->CreateShader( create_info );

    auto vb_attribute = m_cube->GetPositionBufferInfo();

    RHIPrimitiveBufferLayout vb_layout = {};
    vb_layout.attributes = &vb_attribute;
    vb_layout.attributes_count = 1;
    vb_layout.stride = m_cube->GetPositionBufferStride();

    const RHIPrimitiveBufferLayout* buffers[] = { &vb_layout };

    RHIPrimitiveFrequency frequencies[] = { RHIPrimitiveFrequency::PerVertex };

    static_assert( _countof( buffers ) == _countof( frequencies ) );

    RHIInputAssemblerInfo ia_info = {};
    ia_info.buffers_count = _countof( buffers );
    ia_info.primitive_buffers = buffers;
    ia_info.frequencies = frequencies;

    RHIGraphicsPipelineInfo rhi_pipeline_info = {};

    rhi_pipeline_info.input_assembler = &ia_info;
    rhi_pipeline_info.vs = triangle_shader_vs.get();
    rhi_pipeline_info.ps = triangle_shader_ps.get();

    RHIPipelineRTInfo rt_info = {};
    rt_info.format = m_swapchain->GetFormat();
    rhi_pipeline_info.rts_count = 1;
    rhi_pipeline_info.rt_info = &rt_info;
    rhi_pipeline_info.binding_layout = m_shader_bindings_layout.get();

    rhi_pipeline_info.rasterizer.cull_mode = RHICullModeFlags::Back;

    m_cube_graphics_pipeline = m_rhi->CreatePSO( rhi_pipeline_info );
}

void SandboxApp::CreateVertexBuffer()
{
    std::vector<Vertex> vertices =
    {
        {{-0.5f, -0.5f}, {1, 1, 0}, {1, 0}},
        {{0.5f, -0.5f}, {0, 1, 0}, {0, 0}},
        {{0.5f, 0.5f}, {0, 1, 1}, {0, 1}},
        {{-0.5f, 0.5f}, {1, 0, 1}, {1, 1}}
    };

    size_t buffer_size = sizeof( vertices[0] ) * vertices.size();

    RHI::BufferInfo buf_info = {};
    buf_info.size = buffer_size;
    buf_info.usage = RHIBufferUsageFlags::VertexBuffer;

    m_vertex_buffer = RHIUtils::CreateInitializedGPUBuffer( buf_info, vertices.data(), buffer_size );
}

void SandboxApp::CreatePipeline()
{
    RHI::ShaderCreateInfo create_info = {};
    std::wstring shader_path = ToWString( ToOSPath( "#engine/shaders/RHIDemo.hlsl" ).c_str() );
    create_info.filename = shader_path.c_str();

    create_info.frequency = RHI::ShaderFrequency::Vertex;
    create_info.entry_point = "TriangleVS";
    RHIObjectPtr<RHIShader> triangle_shader_vs = m_rhi->CreateShader( create_info );

    create_info.frequency = RHI::ShaderFrequency::Pixel;
    create_info.entry_point = "TrianglePS";
    RHIObjectPtr<RHIShader> triangle_shader_ps = m_rhi->CreateShader( create_info );

    auto vb_attributes = Vertex::GetRHIAttributes();

    RHIPrimitiveBufferLayout vb_layout = {};
    vb_layout.attributes = vb_attributes.data();
    vb_layout.attributes_count = vb_attributes.size();
    vb_layout.stride = sizeof( Vertex );

    const RHIPrimitiveBufferLayout* buffers[] = { &vb_layout };

    RHIPrimitiveFrequency frequencies[] = { RHIPrimitiveFrequency::PerVertex };

    static_assert( _countof( buffers ) == _countof( frequencies ) );

    RHIInputAssemblerInfo ia_info = {};
    ia_info.buffers_count = _countof( buffers );
    ia_info.primitive_buffers = buffers;
    ia_info.frequencies = frequencies;

    RHIGraphicsPipelineInfo rhi_pipeline_info = {};

    rhi_pipeline_info.input_assembler = &ia_info;
    rhi_pipeline_info.vs = triangle_shader_vs.get();
    rhi_pipeline_info.ps = triangle_shader_ps.get();

    RHIPipelineRTInfo rt_info = {};
    rt_info.format = m_swapchain->GetFormat();
    rhi_pipeline_info.rts_count = 1;
    rhi_pipeline_info.rt_info = &rt_info;
    rhi_pipeline_info.binding_layout = m_shader_bindings_layout.get();

    rhi_pipeline_info.rasterizer.cull_mode = RHICullModeFlags::Back;

    m_rhi_graphics_pipeline = m_rhi->CreatePSO( rhi_pipeline_info );
}

void SandboxApp::RecordCommandBuffer( RHICommandList& list, RHISwapChain& swapchain )
{
    list.Begin();

    if ( !m_tlas.Build( list ) )
    {
        SE_LOG_ERROR( Sandbox, "Couldn't build a tlas!" );
    }

    TransitionImageLayout( list, *swapchain.GetTexture(), RHITextureLayout::Present, RHITextureLayout::RenderTarget );

    RHIPassRTVInfo rt = {};
    rt.load_op = RHILoadOp::Clear;
    rt.store_op = RHIStoreOp::Store;
    rt.rtv = m_swapchain->GetRTV();
    rt.clear_value.float32[3] = 1.0f;

    RHIPassInfo pass_info = {};
    pass_info.render_area = RHIRect2D{ .offset = glm::ivec2( 0,0 ), .extent = m_swapchain->GetExtent() };
    pass_info.render_targets = &rt;
    pass_info.render_targets_count = 1;
    list.BeginPass( pass_info );

    RHIViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    glm::uvec2 swapchain_extent = m_swapchain->GetExtent();
    viewport.width = float( swapchain_extent.x );
    viewport.height = float( swapchain_extent.y );
    viewport.min_depth = 0.0f;
    viewport.max_depth = 1.0f;

    RHIRect2D scissor = {};
    scissor.offset = { 0, 0 };
    scissor.extent = swapchain_extent;

    list.SetViewports( 0, &viewport, 1 );
    list.SetScissors( 0, &scissor, 1 );

    if ( m_show_cube )
        list.SetPSO( *m_cube_graphics_pipeline );
    else
        list.SetPSO( *m_rhi_graphics_pipeline );

    list.BindDescriptorSet( 0, *m_binding_tables[GetCurrentFrameIdx()]);

    if ( m_show_cube )
    {
        list.SetVertexBuffers( 0, m_cube->GetVertexBuffer(), 1, nullptr );
        list.SetIndexBuffer( *m_cube->GetIndexBuffer(), m_cube->GetIndexBufferType(), 0 );

        list.DrawIndexed( m_cube->GetNumIndices(), 1, 0, 0, 0 );
    }
    else
    {
        list.SetVertexBuffers( 0, m_vertex_buffer.get(), 1, nullptr );
        list.SetIndexBuffer( *m_index_buffer, RHIIndexBufferType::UInt16, 0 );

        list.DrawIndexed( 6, 1, 0, 0, 0 );
    }

    list.EndPass();

    TransitionImageLayout( list, *swapchain.GetTexture(), RHITextureLayout::RenderTarget, RHITextureLayout::Present );

    list.End();
}

void SandboxApp::OnDrawFrame( std::vector<RHICommandList*>& lists_to_submit )
{
    RHICommandList* cmd_list = m_rhi->GetCommandList( RHI::QueueType::Graphics );

    UpdateUniformBuffer( GetCurrentFrameIdx() );

    RecordCommandBuffer( *cmd_list, *m_swapchain );

    lists_to_submit.emplace_back( cmd_list );
}

void SandboxApp::OnUpdate()
{
    UpdateGui();
}

void SandboxApp::UpdateGui()
{
    if ( ImGui::BeginMainMenuBar() )
    {
        if ( ImGui::MenuItem( "WorldOutliner" ) )
            m_show_world_outliner = true;

        if ( ImGui::MenuItem( "ImGUIDemo" ) )
            m_show_imgui_demo = true;

        if ( ImGui::MenuItem( "ReloadShaders" ) )
            m_rhi->ReloadAllShaders();

        ImGui::EndMainMenuBar();
    }

    if ( m_show_imgui_demo )
        ImGui::ShowDemoWindow( &m_show_imgui_demo );

    if ( m_show_world_outliner )
    {
        ImGui::Begin( "World", &m_show_world_outliner, ImGuiWindowFlags_None );
        {
            ImGui::Text( "Total: %llu entities", m_world.GetEntityCount() );
            bool create_entity = ImGui::Button( "Add enitity" );
            ImGui::SameLine();
            ImGui::InputText( "Name", &m_new_entity_name );

            if ( create_entity )
            {
                auto new_entity = m_world.CreateEntity();
                auto& name_component = m_world.AddComponent<NameComponent>( new_entity );
                name_component.name = std::move( m_new_entity_name );
            }

            for ( auto&& [id, name_comp] : m_world.CreateView<NameComponent>() )
            {
                ImGui::Text( name_comp.name.c_str() );
            }
        }
        ImGui::End();
    }

    ImGui::Begin( "Demo" );
    {
        ImGui::Checkbox( "Show cube", &m_show_cube );
    }
    ImGui::End();
}
