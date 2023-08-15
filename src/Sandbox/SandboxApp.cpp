#include "StdAfx.h"

#include "SandboxApp.h"

#include <Engine/Assets.h>
#include <Engine/RHIUtils.h>
#include <Engine/Rendergraph.h>

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

void SandboxApp::ParseCommandLineDerived( int argc, char** argv )
{
    for ( int i = 1; i < argc; ++i )
    {
        if ( !_strcmpi( argv[i], "-DemoAssetPath" ) )
        {
            if ( argc > ( i + 1 ) )
            {
                // read next arg
                m_demo_mesh_asset_path = argv[i + 1];
                i += 1;


                SE_LOG_INFO( Sandbox, "Set demo mesh path from command line: %s", m_demo_mesh_asset_path );
            }
            else
            {
                SE_LOG_ERROR( Sandbox, "Can't set demo asset path from command line because -DemoAssetPath is the last token" );
            }
        }
    }
}

void SandboxApp::OnInit()
{
    SE_LOG_INFO( Sandbox, "Sandbox initialization started" );

    m_demo_mesh = boost::dynamic_pointer_cast< MeshAsset >( m_asset_mgr->Load( AssetId( m_demo_mesh_asset_path.c_str() ) ) );
    if ( !( m_demo_mesh && m_demo_mesh->GetStatus() == AssetStatus::Ready ) )
    {
        SE_LOG_FATAL_ERROR( Sandbox, "Could not load demo asset an path %s", m_demo_mesh_asset_path.c_str() );
    }

    m_scene = std::make_unique<Scene>();

    CreateIntermediateBuffers();
    
    CreateDescriptorSetLayout();
    CreatePipeline();
    CreateCubePipeline();
    CreateFullscreenQuadPipeline();
    if ( m_rhi->SupportsRaytracing() )
    {
        CreateRTPipeline();
    }

    CreateTextureImage();
    CreateTextureImageView();
    CreateTextureSampler();
    CreateVertexBuffer();
    CreateIndexBuffer();
    CreateUniformBuffers();
    CreateDescriptorSets();

    m_demo_mesh_entity = m_world.CreateEntity();
    m_world.AddComponent<NameComponent>( m_demo_mesh_entity ).name = "DemoMesh";
    m_world.AddComponent<TransformComponent>( m_demo_mesh_entity );
    auto& demo_mesh_component = m_world.AddComponent<MeshInstanceComponent>( m_demo_mesh_entity );
    demo_mesh_component.scene_mesh_instance = m_scene->AddMeshInstanceFromAsset( m_demo_mesh );

    SE_LOG_INFO( Sandbox, "Sandbox initialization complete" );
}

void SandboxApp::OnCleanup()
{
    SE_LOG_INFO( Sandbox, "Sandbox shutdown started" );

    m_binding_tables.clear();
    m_rt_descsets.clear();
    m_fsquad_descsets.clear();
    m_uniform_buffer_views.clear();
    m_uniform_buffers.clear();
    m_index_buffer = nullptr;
    m_vertex_buffer = nullptr;

    m_texture_sampler = nullptr;
    m_texture_srv = nullptr;
    m_texture = nullptr;

    m_point_sampler = nullptr;
    m_frame_rwview = nullptr;
    m_frame_roview = nullptr;
    m_rt_frame = nullptr;

    CleanupPipeline();

    m_vertex_buffer = nullptr;
    m_index_buffer = nullptr;
    m_uniform_buffers.clear();

    m_scene = nullptr;

    m_demo_mesh = nullptr;

    SE_LOG_INFO( Sandbox, "Sandbox shutdown complete" );
}

void SandboxApp::CleanupPipeline()
{
    m_rt_pipeline = nullptr;
    m_rt_layout = nullptr;
    m_rt_dsl = nullptr;
    m_rhi_graphics_pipeline = nullptr;
    m_binding_table_layout = nullptr;
    m_shader_bindings_layout = nullptr;
    m_cube_graphics_pipeline = nullptr;
    m_draw_fullscreen_quad_pipeline = nullptr;
    m_shader_bindings_layout_fsquad = nullptr;
    m_dsl_fsquad = nullptr;
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
    {
        RHI::DescriptorViewRange ranges[3] = {};
        ranges[0].type = RHIShaderBindingType::UniformBuffer;
        ranges[0].count = 1;
        ranges[0].stages = RHIShaderStageFlags::VertexShader;

        ranges[1].type = RHIShaderBindingType::TextureRO;
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

    if ( m_rhi->SupportsRaytracing() )
    {
        RHI::DescriptorViewRange ranges[3] = {};
        ranges[0].type = RHIShaderBindingType::AccelerationStructure;
        ranges[0].count = 1;
        ranges[0].stages = RHIShaderStageFlags::RaygenShader;

        ranges[1].type = RHIShaderBindingType::TextureRW;
        ranges[1].count = 1;
        ranges[1].stages = RHIShaderStageFlags::RaygenShader;

        ranges[2].type = RHIShaderBindingType::UniformBuffer;
        ranges[2].count = 1;
        ranges[2].stages = RHIShaderStageFlags::RaygenShader;

        RHI::DescriptorSetLayoutInfo binding_table = {};
        binding_table.ranges = ranges;
        binding_table.range_count = std::size( ranges );

        m_rt_dsl = m_rhi->CreateDescriptorSetLayout( binding_table );

        RHI::ShaderBindingLayoutInfo rt_layout_info = {};

        RHIDescriptorSetLayout* rt_dsls[1] = { m_rt_dsl.get() };
        rt_layout_info.tables = rt_dsls;
        rt_layout_info.table_count = std::size( rt_dsls );

        m_rt_layout = m_rhi->CreateShaderBindingLayout( rt_layout_info );
    }

    {
        RHI::DescriptorViewRange ranges[2] = {};

        ranges[0].type = RHIShaderBindingType::TextureRO;
        ranges[0].count = 1;
        ranges[0].stages = RHIShaderStageFlags::PixelShader;

        ranges[1].type = RHIShaderBindingType::Sampler;
        ranges[1].count = 1;
        ranges[1].stages = RHIShaderStageFlags::PixelShader;

        RHI::DescriptorSetLayoutInfo binding_table = {};
        binding_table.ranges = ranges;
        binding_table.range_count = std::size( ranges );

        m_dsl_fsquad = m_rhi->CreateDescriptorSetLayout( binding_table );

        RHI::ShaderBindingLayoutInfo layout_info = {};
        RHIDescriptorSetLayout* dsls[1] = { m_dsl_fsquad.get() };
        layout_info.tables = dsls;
        layout_info.table_count = std::size( dsls );

        m_shader_bindings_layout_fsquad = m_rhi->CreateShaderBindingLayout( layout_info );
    }
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
        RHI::UniformBufferViewInfo view_info = {};
        view_info.buffer = m_uniform_buffers[i]->GetBuffer();
        m_uniform_buffer_views[i] = m_rhi->CreateUniformBufferView( view_info );
    }
}

void SandboxApp::UpdateUniformBuffer( uint32_t current_image )
{
    static auto start_time = std::chrono::high_resolution_clock::now();

    auto current_time = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>( current_time - start_time ).count();

    const auto* demo_tf = m_world.GetComponent<TransformComponent>( m_demo_mesh_entity );
    const auto* demo_mesh = m_world.GetComponent<MeshInstanceComponent>( m_demo_mesh_entity );

    Matrices matrices = {};

    matrices.view = glm::lookAt( glm::vec3( 2, 2, 2 ), glm::vec3( 0, 0, 0 ), glm::vec3( 0, 1, 0 ) );

    glm::uvec2 swapchain_extent = m_swapchain->GetExtent();
    matrices.proj = glm::perspective( glm::radians( m_fov_degrees ), float( swapchain_extent.x ) / float( swapchain_extent.y ), 0.1f, 10.0f );

    matrices.viewport_size = swapchain_extent;

    glm::mat4x4 view_proj = matrices.proj * matrices.view;

    matrices.view_proj_inv = glm::inverse( view_proj );

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
        m_binding_tables[i]->BindUniformBufferView( 0, 0, *m_uniform_buffer_views[i] );
        m_binding_tables[i]->BindTextureROView( 1, 0, *m_texture_srv );
        m_binding_tables[i]->BindSampler( 2, 0, *m_texture_sampler );
        m_binding_tables[i]->FlushBinds(); // optional, will be flushed anyway on cmdlist.BindDescriptorSet
    }

    m_rt_descsets.resize( m_max_frames_in_flight );
    for ( size_t i = 0; i < m_max_frames_in_flight; ++i )
    {
        m_rt_descsets[i] = m_rhi->CreateDescriptorSet( *m_rt_dsl );
        m_rt_descsets[i]->BindUniformBufferView( 2, 0, *m_uniform_buffer_views[i] );
        m_rt_descsets[i]->FlushBinds(); // optional, will be flushed anyway on cmdlist.BindDescriptorSet
    }

    m_fsquad_descsets.resize( m_max_frames_in_flight );
    for ( size_t i = 0; i < m_max_frames_in_flight; ++i )
    {
        m_fsquad_descsets[i] = m_rhi->CreateDescriptorSet(*m_dsl_fsquad);
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
        RHITextureUsageFlags::TextureROView | RHITextureUsageFlags::TransferDst,
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
    RHI::TextureROViewInfo srv_info = {};
    srv_info.texture = m_texture.get();
    m_texture_srv = m_rhi->CreateTextureROView( srv_info );
}

void SandboxApp::CreateTextureSampler()
{
    RHI::SamplerInfo info = {};
    info.enable_anisotropy = true;
    m_texture_sampler = m_rhi->CreateSampler( info );


    RHI::SamplerInfo info_point = {};
    m_point_sampler = m_rhi->CreateSampler( info_point );
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

    auto vb_attribute = m_demo_mesh->GetPositionBufferInfo();

    RHIPrimitiveBufferLayout vb_layout = {};
    vb_layout.attributes = &vb_attribute;
    vb_layout.attributes_count = 1;
    vb_layout.stride = m_demo_mesh->GetPositionBufferStride();

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

void SandboxApp::CreateRTPipeline()
{
    RHI::ShaderCreateInfo create_info = {};
    std::wstring shader_path = ToWString( ToOSPath( "#engine/shaders/Raytracing.hlsl" ).c_str() );
    create_info.filename = shader_path.c_str();

    create_info.frequency = RHI::ShaderFrequency::Raygen;
    create_info.entry_point = "VisibilityRGS";
    RHIObjectPtr<RHIShader> raygen_shader = m_rhi->CreateShader( create_info );

    create_info.entry_point = "VisibilityRMS";
    create_info.frequency = RHI::ShaderFrequency::Miss;
    RHIObjectPtr<RHIShader> miss_shader = m_rhi->CreateShader( create_info );

    create_info.entry_point = "VisibilityRCS";
    create_info.frequency = RHI::ShaderFrequency::ClosestHit;
    RHIObjectPtr<RHIShader> closest_hit_shader = m_rhi->CreateShader( create_info );

    RHIRaytracingPipelineInfo rt_pipeline_info = {};
    rt_pipeline_info.raygen_shader = raygen_shader.get();
    rt_pipeline_info.miss_shader = miss_shader.get();
    rt_pipeline_info.closest_hit_shader = closest_hit_shader.get();
    rt_pipeline_info.binding_layout = m_rt_layout.get();

    m_rt_pipeline = m_rhi->CreatePSO( rt_pipeline_info );
}

void SandboxApp::CreateFullscreenQuadPipeline()
{
    RHI::ShaderCreateInfo create_info = {};
    std::wstring shader_path = ToWString( ToOSPath( "#engine/shaders/FullscreenQuad.hlsl" ).c_str() );
    create_info.filename = shader_path.c_str();

    create_info.frequency = RHI::ShaderFrequency::Vertex;
    create_info.entry_point = "FullScreenQuadVS";
    RHIObjectPtr<RHIShader> triangle_shader_vs = m_rhi->CreateShader( create_info );

    create_info.frequency = RHI::ShaderFrequency::Pixel;
    create_info.entry_point = "FullScreenQuadPS";
    RHIObjectPtr<RHIShader> triangle_shader_ps = m_rhi->CreateShader( create_info );

    RHIInputAssemblerInfo ia_info = {};

    RHIGraphicsPipelineInfo rhi_pipeline_info = {};

    rhi_pipeline_info.input_assembler = &ia_info;
    rhi_pipeline_info.vs = triangle_shader_vs.get();
    rhi_pipeline_info.ps = triangle_shader_ps.get();

    RHIPipelineRTInfo rt_info = {};
    rt_info.format = m_swapchain->GetFormat();
    rhi_pipeline_info.rts_count = 1;
    rhi_pipeline_info.rt_info = &rt_info;
    rhi_pipeline_info.binding_layout = m_shader_bindings_layout_fsquad.get();

    rhi_pipeline_info.rasterizer.cull_mode = RHICullModeFlags::None;

    m_draw_fullscreen_quad_pipeline = m_rhi->CreatePSO( rhi_pipeline_info );
}

void SandboxApp::CreateIntermediateBuffers()
{
    CreateImage( m_swapchain->GetExtent().x, m_swapchain->GetExtent().y, RHIFormat::R8G8B8A8_UNORM, RHITextureUsageFlags::TextureROView | RHITextureUsageFlags::TextureRWView, RHITextureLayout::ShaderReadOnly, m_rt_frame );
    
    RHI::TextureRWViewInfo uav_info = {};
    uav_info.texture = m_rt_frame.get();
    m_frame_rwview = m_rhi->CreateTextureRWView( uav_info );

    RHI::TextureROViewInfo srv_info = {};
    srv_info.texture = m_rt_frame.get();
    m_frame_roview = m_rhi->CreateTextureROView( srv_info );
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

void SandboxApp::BuildRendergraphRT( Rendergraph& rendergraph, RHICommandList* ui_cmd_list )
{
    // 1. Setup stage. Setup your resource handles and passes
    RGExternalTextureDesc swapchain_desc = {};
    swapchain_desc.name = "swapchain";
    swapchain_desc.rhi_texture = m_swapchain->GetTexture();
    swapchain_desc.initial_layout = RHITextureLayout::Present;
    swapchain_desc.final_layout = RHITextureLayout::Present;

    RGExternalTexture* swapchain = rendergraph.RegisterExternalTexture( swapchain_desc );

    RGExternalTextureDesc frame_output_desc = {};
    frame_output_desc.name = "frame_output";
    frame_output_desc.rhi_texture = m_rt_frame.get();
    frame_output_desc.initial_layout = RHITextureLayout::ShaderReadOnly;
    frame_output_desc.final_layout = RHITextureLayout::ShaderReadOnly;

    RGExternalTexture* frame_output = rendergraph.RegisterExternalTexture( frame_output_desc );

    RGPass* update_as_pass = rendergraph.AddPass( RHI::QueueType::Graphics, "UpdateAS" );

    RGPass* rt_pass = rendergraph.AddPass( RHI::QueueType::Graphics, "RaytraceScene" );
    rt_pass->UseTexture( *frame_output, RGTextureUsage::ShaderReadWrite );

    RGPass* blit_to_swapchain = rendergraph.AddPass( RHI::QueueType::Graphics, "BlitToSwapchain" );
    blit_to_swapchain->UseTexture( *swapchain, RGTextureUsage::RenderTarget );
    blit_to_swapchain->UseTexture( *frame_output, RGTextureUsage::ShaderRead );

    RGPass* ui_pass = rendergraph.AddPass( RHI::QueueType::Graphics, "DrawUI" );
    ui_pass->UseTexture( *swapchain, RGTextureUsage::RenderTarget );

    // 2. Compile stage. That will create a timeline for each resource, allowing us to fetch real RHI handles for them inside passes
    rendergraph.Compile();

    // 3. Record phase. Fill passes with command lists
    RHICommandList* cmd_list_rt = GetRHI().GetCommandList( RHI::QueueType::Graphics );

    cmd_list_rt->Begin();

    update_as_pass->AddCommandList( *cmd_list_rt );
    {
        if ( !m_scene->GetTLAS().Build(*cmd_list_rt) )
        {
            SE_LOG_ERROR( Sandbox, "Couldn't build a tlas!" );
        }
    }
    update_as_pass->EndPass();

    rt_pass->BorrowCommandList( *cmd_list_rt );
    {
        m_rt_descsets[GetCurrentFrameIdx()]->BindAccelerationStructure( 0, 0, m_scene->GetTLAS().GetRHIAS() );
        m_rt_descsets[GetCurrentFrameIdx()]->BindTextureRWView( 1, 0, *m_frame_rwview );

        cmd_list_rt->SetPSO( *m_rt_pipeline );

        cmd_list_rt->BindDescriptorSet( 0, *m_rt_descsets[GetCurrentFrameIdx()] );

        cmd_list_rt->TraceRays( glm::uvec3( m_swapchain->GetExtent(), 1 ) );
    }
    rt_pass->EndPass();

    blit_to_swapchain->BorrowCommandList( *cmd_list_rt );
    {
        m_fsquad_descsets[GetCurrentFrameIdx()]->BindTextureROView( 0, 0, *m_frame_roview );
        m_fsquad_descsets[GetCurrentFrameIdx()]->BindSampler( 1, 0, *m_point_sampler );

        RHIPassRTVInfo rt = {};
        rt.load_op = RHILoadOp::Clear;
        rt.store_op = RHIStoreOp::Store;
        rt.rtv = m_swapchain->GetRTV();
        rt.clear_value.float32[3] = 1.0f;

        RHIPassInfo pass_info = {};
        pass_info.render_area = RHIRect2D{ .offset = glm::ivec2( 0,0 ), .extent = m_swapchain->GetExtent() };
        pass_info.render_targets = &rt;
        pass_info.render_targets_count = 1;
        cmd_list_rt->BeginPass( pass_info );

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

        cmd_list_rt->SetViewports( 0, &viewport, 1 );
        cmd_list_rt->SetScissors( 0, &scissor, 1 );

        cmd_list_rt->SetPSO( *m_draw_fullscreen_quad_pipeline );

        cmd_list_rt->BindDescriptorSet( 0, *m_fsquad_descsets[GetCurrentFrameIdx()] );

        cmd_list_rt->Draw( 3, 1, 0, 0 );

        cmd_list_rt->EndPass();
    }
    blit_to_swapchain->EndPass();   

    // Dirty hack. ui_pass may want to add extra commands at the start of it's first command list, but ui_cmd_list is already filled. Borrow list from last pass to insert those commands
    ui_pass->BorrowCommandList( *cmd_list_rt );
    cmd_list_rt->End();

    if ( ui_cmd_list != nullptr )
    {
        ui_pass->AddCommandList( *ui_cmd_list );
    }
    ui_pass->EndPass();
}

void SandboxApp::OnDrawFrame( Rendergraph& framegraph, RHICommandList* ui_cmd_list )
{
    m_scene->Synchronize();

    UpdateUniformBuffer( GetCurrentFrameIdx() );

    BuildRendergraphRT( framegraph, ui_cmd_list );
}

void SandboxApp::OnUpdate()
{
    UpdateGui();

    static auto start_time = std::chrono::high_resolution_clock::now();

    auto current_time = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>( current_time - start_time ).count();

    auto* demo_tf = m_world.GetComponent<TransformComponent>( m_demo_mesh_entity );
    if ( !SE_ENSURE( demo_tf ) )
        return;

    demo_tf->tf.orientation = glm::angleAxis( time * glm::radians( 90.0f ), glm::vec3( 0, 1, 0 ) );

    UpdateScene();
}

void SandboxApp::UpdateScene()
{
    // this should be somewhere in the engine code

    // @todo - run only for dirty transforms
    for ( const auto& [entity_id, tf, mesh_instance_component] : m_world.CreateView<TransformComponent, MeshInstanceComponent>() )
    {
        SceneMeshInstance* smi = m_scene->GetMeshInstance( mesh_instance_component.scene_mesh_instance );
        if ( !SE_ENSURE( smi ) )
            continue;

        smi->m_tf = tf.tf;
    }
}

void SandboxApp::OnSwapChainRecreated()
{
    CreateIntermediateBuffers();
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
        ImGui::SliderFloat( "FoV (degrees)", &m_fov_degrees, 1.0f, 179.0f, "%.1f" );
    }
    ImGui::End();
}
