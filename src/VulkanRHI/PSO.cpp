#include "StdAfx.h"

#include "PSO.h"

#include "VulkanRHIImpl.h"

IMPLEMENT_RHI_OBJECT( VulkanGraphicsPSO )

VulkanGraphicsPSO::VulkanGraphicsPSO( VulkanRHI* rhi, const RHIGraphicsPipelineInfo& info )
    : m_rhi( rhi ), m_vs( static_cast< Shader* >( info.vs ) ), m_ps( static_cast< Shader* >( info.ps ) )
{
    m_pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    int i = 0;

    if ( m_vs )
        m_stages[i++] = m_vs->GetVkStageInfo();

    if ( m_ps )
        m_stages[i++] = m_ps->GetVkStageInfo();

    m_pipeline_info.pStages = m_stages;
    m_pipeline_info.stageCount = i;

    m_pipeline_info.stageCount = 2;
    m_pipeline_info.pStages = m_stages;

    InitInputAssembly( info );
    InitDynamicState( info );
    InitRasterizer( info );
    InitMultisampling( info );
    m_pipeline_info.pDepthStencilState = nullptr;
    InitDynamicRendering( info );
    InitColorBlending( info );

    m_pipeline_info.renderPass = nullptr;
    m_pipeline_info.subpass = 0;

    m_pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    m_pipeline_info.basePipelineIndex = -1;

    VERIFY_NOT_EQUAL( info.binding_layout, nullptr );
    m_shader_bindings = &RHIImpl( *info.binding_layout );

    m_pipeline_info.layout = m_shader_bindings->GetVkPipelineLayout();

    VK_VERIFY( vkCreateGraphicsPipelines( m_rhi->GetDevice(), VK_NULL_HANDLE, 1, &m_pipeline_info, nullptr, &m_vk_pipeline ) );

    m_rhi->RegisterLoadedPSO( *this );
}

VulkanGraphicsPSO::~VulkanGraphicsPSO()
{
    m_rhi->UnregisterLoadedPSO( *this );

    vkDestroyPipeline( m_rhi->GetDevice(), m_vk_pipeline, nullptr );
}

bool VulkanGraphicsPSO::Recompile()
{
    // assumes only shader stages may change

    int i = 0;

    if ( m_vs )
        m_stages[i++] = m_vs->GetVkStageInfo();

    if ( m_ps )
        m_stages[i++] = m_ps->GetVkStageInfo();

    m_pipeline_info.pStages = m_stages;
    m_pipeline_info.stageCount = i;

    VkPipeline new_pipeline = VK_NULL_HANDLE;
    VkResult res = vkCreateGraphicsPipelines( m_rhi->GetDevice(), VK_NULL_HANDLE, 1, &m_pipeline_info, nullptr, &new_pipeline );

    if ( res != VK_SUCCESS )
        return false;

    vkDestroyPipeline( m_rhi->GetDevice(), m_vk_pipeline, nullptr );
    m_vk_pipeline = new_pipeline;

    return res == VK_SUCCESS;
}

void VulkanGraphicsPSO::InitRasterizer( const RHIGraphicsPipelineInfo& info )
{
    m_rasterizer_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    m_rasterizer_info.depthClampEnable = VK_FALSE;
    m_rasterizer_info.rasterizerDiscardEnable = VK_FALSE;
    m_rasterizer_info.polygonMode = VK_POLYGON_MODE_FILL;
    m_rasterizer_info.cullMode = VulkanRHI::GetCullModeFlags( info.rasterizer.cull_mode );
    m_rasterizer_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    m_rasterizer_info.depthBiasEnable = VK_FALSE;
    m_rasterizer_info.lineWidth = 1.0f;

    m_pipeline_info.pRasterizationState = &m_rasterizer_info;
}

void VulkanGraphicsPSO::InitInputAssembly( const RHIGraphicsPipelineInfo& info )
{
    m_input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    m_input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    m_input_assembly.primitiveRestartEnable = VK_FALSE;

    VERIFY_NOT_EQUAL( info.input_assembler, nullptr );

    size_t attribute_count = 0;
    for ( size_t i = 0; i < m_binding_descs.size(); ++i )
    {
        attribute_count += info.input_assembler->primitive_buffers[i]->attributes_count;
    }
    m_attribute_descs.reserve( attribute_count );

    m_binding_descs.resize( info.input_assembler->buffers_count );
    for ( uint32_t i = 0; i < uint32_t( m_binding_descs.size() ); ++i )
    {
        m_binding_descs[i].binding = i;
        m_binding_descs[i].inputRate = VulkanRHI::GetVkVertexInputRate( info.input_assembler->frequencies[i] );

        VERIFY_NOT_EQUAL( info.input_assembler->primitive_buffers[i], nullptr );

        const RHIPrimitiveBufferLayout& buffer_layout = *info.input_assembler->primitive_buffers[i];

        for ( uint32_t j = 0; j < uint32_t( buffer_layout.attributes_count ); ++j )
        {
            uint32_t location = uint32_t( m_attribute_descs.size() );
            VkVertexInputAttributeDescription& vk_attr_desc = m_attribute_descs.emplace_back();
            vk_attr_desc.binding = i;
            vk_attr_desc.format = VulkanRHI::GetVkFormat( buffer_layout.attributes[j].format );
            vk_attr_desc.location = location;
            vk_attr_desc.offset = buffer_layout.attributes[j].offset;
        }

        m_binding_descs[i].stride = uint32_t( buffer_layout.stride );
    }

    m_vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    m_vertex_input_info.vertexBindingDescriptionCount = uint32_t( info.input_assembler->buffers_count );
    m_vertex_input_info.pVertexBindingDescriptions = m_binding_descs.data();
    m_vertex_input_info.vertexAttributeDescriptionCount = uint32_t( m_attribute_descs.size() );
    m_vertex_input_info.pVertexAttributeDescriptions = m_attribute_descs.data();

    m_pipeline_info.pVertexInputState = &m_vertex_input_info;
    m_pipeline_info.pInputAssemblyState = &m_input_assembly;
}

void VulkanGraphicsPSO::InitDynamicState( const RHIGraphicsPipelineInfo& info )
{
    m_dynamic_states =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    m_dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    m_dynamic_state_info.dynamicStateCount = uint32_t( m_dynamic_states.size() );
    m_dynamic_state_info.pDynamicStates = m_dynamic_states.data();

    m_pipeline_info.pDynamicState = &m_dynamic_state_info;

    m_viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    m_viewport_info.viewportCount = uint32_t( info.rts_count );
    m_viewport_info.pViewports = nullptr;
    m_viewport_info.scissorCount = uint32_t( info.rts_count );
    m_viewport_info.pScissors = nullptr;

    m_pipeline_info.pViewportState = &m_viewport_info;
}

void VulkanGraphicsPSO::InitMultisampling( const RHIGraphicsPipelineInfo& info )
{
    m_multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    m_multisampling.sampleShadingEnable = VK_FALSE;
    m_multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    m_multisampling.minSampleShading = 1.0f;
    m_multisampling.pSampleMask = nullptr;
    m_multisampling.alphaToCoverageEnable = VK_FALSE;
    m_multisampling.alphaToOneEnable = VK_FALSE;

    m_pipeline_info.pMultisampleState = &m_multisampling;
}

void VulkanGraphicsPSO::InitColorBlending( const RHIGraphicsPipelineInfo& info )
{
    m_color_blend_attachments.resize( info.rts_count );
    for ( int i = 0; i < info.rts_count; ++i )
    {
        const auto& rt_info = info.rt_info[i];

        auto& attachment = m_color_blend_attachments[i];

        attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        attachment.blendEnable = rt_info.enable_blend ? VK_TRUE : VK_FALSE;

        if ( attachment.blendEnable )
        {
            attachment.blendEnable = VK_TRUE;

            attachment.colorBlendOp = VK_BLEND_OP_ADD;
            attachment.srcColorBlendFactor = VulkanRHI::GetVkBlendFactor( rt_info.src_color_blend );
            attachment.dstColorBlendFactor = VulkanRHI::GetVkBlendFactor( rt_info.dst_color_blend );

            attachment.alphaBlendOp = VK_BLEND_OP_ADD;
            attachment.srcAlphaBlendFactor = VulkanRHI::GetVkBlendFactor( rt_info.src_alpha_blend );
            attachment.dstAlphaBlendFactor = VulkanRHI::GetVkBlendFactor( rt_info.dst_alpha_blend );
        }
        else
        {
            attachment.blendEnable = VK_FALSE;
        }
    }

    m_color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    m_color_blending.logicOpEnable = VK_FALSE;
    m_color_blending.attachmentCount = uint32_t( m_color_blend_attachments.size() );
    m_color_blending.pAttachments = m_color_blend_attachments.data();

    m_pipeline_info.pColorBlendState = &m_color_blending;
}

void VulkanGraphicsPSO::InitDynamicRendering( const RHIGraphicsPipelineInfo& info )
{
    m_pipeline_dynamic_rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

    m_color_attachment_formats.resize( info.rts_count );
    for ( size_t i = 0; i < info.rts_count; ++i )
    {
        m_color_attachment_formats[i] = VulkanRHI::GetVkFormat( info.rt_info[i].format );
    }

    m_pipeline_dynamic_rendering_info.colorAttachmentCount = uint32_t( m_color_attachment_formats.size() );
    m_pipeline_dynamic_rendering_info.pColorAttachmentFormats = m_color_attachment_formats.data();

    m_pipeline_info.pNext = &m_pipeline_dynamic_rendering_info;
}


IMPLEMENT_RHI_OBJECT( VulkanRaytracingPSO )

VulkanRaytracingPSO::VulkanRaytracingPSO( VulkanRHI* rhi, const RHIRaytracingPipelineInfo& info )
    : m_rhi( rhi ), m_rgs( RHIImpl( info.raygen_shader ) )
{
    m_vk_pipeline_ci.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;

    InitShaderStages();

    m_vk_pipeline_ci.maxPipelineRayRecursionDepth = 1;

    VERIFY_NOT_EQUAL( info.binding_layout, nullptr );
    m_shader_bindings = &RHIImpl( *info.binding_layout );

    m_vk_pipeline_ci.layout = m_shader_bindings->GetVkPipelineLayout();

    m_vk_pipeline_ci.basePipelineHandle = VK_NULL_HANDLE;
    m_vk_pipeline_ci.basePipelineIndex = -1;

    VK_VERIFY( vkCreateRayTracingPipelinesKHR( m_rhi->GetDevice(), {}, {}, 1, &m_vk_pipeline_ci, nullptr, &m_vk_pipeline ) );

    InitSBT();

    m_rhi->RegisterLoadedPSO( *this );
}

VulkanRaytracingPSO::~VulkanRaytracingPSO()
{
    m_rhi->UnregisterLoadedPSO( *this );

    vkDestroyPipeline( m_rhi->GetDevice(), m_vk_pipeline, nullptr );
}

bool VulkanRaytracingPSO::Recompile()
{
    InitShaderStages();

    VkPipeline new_pipeline = VK_NULL_HANDLE;
    VkResult res = vkCreateRayTracingPipelinesKHR( m_rhi->GetDevice(), {}, {}, 1, &m_vk_pipeline_ci, nullptr, &new_pipeline );

    if ( res != VK_SUCCESS )
        return false;

    vkDestroyPipeline( m_rhi->GetDevice(), m_vk_pipeline, nullptr );
    m_vk_pipeline = new_pipeline;

    InitSBT();

    return true;
}

namespace
{
    void InitShaderGroup( VkRayTracingShaderGroupCreateInfoKHR& vk_sg )
    {
        vk_sg = {};
        vk_sg.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        vk_sg.anyHitShader = VK_SHADER_UNUSED_KHR;
        vk_sg.closestHitShader = VK_SHADER_UNUSED_KHR;
        vk_sg.generalShader = VK_SHADER_UNUSED_KHR;
        vk_sg.intersectionShader = VK_SHADER_UNUSED_KHR;
        vk_sg.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    }
}

void VulkanRaytracingPSO::InitShaderStages()
{
    m_stages.clear();

    const auto& rt_props = m_rhi->GetFeatures().rt_pipe_props;

    const uint32_t sbt_handle_size = rt_props.shaderGroupHandleSize;
    const uint32_t sbt_handle_size_aligned = uint32_t( CalcAlignedSize( sbt_handle_size, rt_props.shaderGroupHandleAlignment ) );

    size_t sbt_size = 0;

    if ( m_rgs )
    {
        const uint32_t stage_idx = uint32_t( m_stages.size() );
        m_stages.emplace_back( m_rgs->GetVkStageInfo() );
        auto& new_group = m_groups.emplace_back();
        InitShaderGroup( new_group );
        new_group.generalShader = stage_idx;
        new_group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    }

    auto init_region = [&]( auto& region )
    {
        region.stride = sbt_handle_size_aligned;
        region.size = region.stride;
        sbt_size += CalcAlignedSize( sbt_handle_size_aligned, rt_props.shaderGroupBaseAlignment );
    };

    init_region( m_rgs_region );
    init_region( m_miss_region );
    init_region( m_hit_region );
    init_region( m_callable_region );

    RHI::BufferInfo sbt_ci = {};
    sbt_ci.size = sbt_size;
    sbt_ci.usage = RHIBufferUsageFlags::ShaderBindingTable;
    m_sbt = RHIImpl( m_rhi->CreateUploadBuffer( sbt_ci ) );

    m_vk_pipeline_ci.pStages = m_stages.data();
    m_vk_pipeline_ci.stageCount = uint32_t( m_stages.size() );

    m_vk_pipeline_ci.pGroups = m_groups.data();
    m_vk_pipeline_ci.groupCount = uint32_t( m_groups.size() );
}

void VulkanRaytracingPSO::InitSBT()
{
    if ( !SE_ENSURE( m_sbt != nullptr ) )
        return;

    const VkDeviceAddress base_dev_addr = RHIImpl( m_sbt->GetBuffer() )->GetDeviceAddress();

    const auto& rt_props = m_rhi->GetFeatures().rt_pipe_props;
    const uint32_t sbt_handle_size = rt_props.shaderGroupHandleSize;

    uint32_t data_size = uint32_t( m_groups.size() ) * sbt_handle_size;
    bc::small_vector<uint8_t, SizeKB> handles( data_size );
    VK_VERIFY( vkGetRayTracingShaderGroupHandlesKHR( m_rhi->GetDevice(), m_vk_pipeline, 0, uint32_t( m_groups.size() ), data_size, handles.data() ) );

    size_t cur_handle_idx = 0;
    size_t dev_address_offset = 0;

    // rgs entry
    {
        m_rgs_region.deviceAddress = base_dev_addr + dev_address_offset;
        dev_address_offset += CalcAlignedSize( m_rgs_region.size, rt_props.shaderGroupBaseAlignment );
        if ( m_rgs )
        {
            m_sbt->WriteBytes( handles.data() + sbt_handle_size * cur_handle_idx, sbt_handle_size, dev_address_offset );
            cur_handle_idx++;
        }
    }

    // miss entry
    {
        m_miss_region.deviceAddress = base_dev_addr + dev_address_offset;
        dev_address_offset += CalcAlignedSize( m_miss_region.size, rt_props.shaderGroupBaseAlignment );
        // if (miss_shader)
    }

    // hit entry
    {
        m_hit_region.deviceAddress = base_dev_addr + dev_address_offset;
        dev_address_offset += CalcAlignedSize( m_hit_region.size, rt_props.shaderGroupBaseAlignment );
    }

    // callable entry
    {
        m_callable_region.deviceAddress = base_dev_addr + dev_address_offset;
        dev_address_offset += CalcAlignedSize( m_callable_region.size, rt_props.shaderGroupBaseAlignment );
    }   

}
