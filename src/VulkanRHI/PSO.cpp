#include "StdAfx.h"

#include "PSO.h"

#include "VulkanRHIImpl.h"

VulkanGraphicsPSO::VulkanGraphicsPSO(VulkanRHI* rhi, const RHIGraphicsPipelineInfo& info)
	: m_rhi(rhi), m_vs(static_cast<Shader*>(info.vs)), m_ps(static_cast<Shader*>(info.ps))
{
	m_pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

	m_stages[0] = m_vs->GetVkStageInfo();
	m_stages[1] = m_ps->GetVkStageInfo();

	m_pipeline_info.stageCount = 2;
	m_pipeline_info.pStages = m_stages;

	InitInputAssembly(info);
	InitDynamicState(info);
	InitRasterizer(info);
	InitMultisampling(info);
	m_pipeline_info.pDepthStencilState = nullptr;
	InitDynamicRendering(info);
	InitColorBlending(info);

	m_pipeline_info.renderPass = nullptr;
	m_pipeline_info.subpass = 0;

	m_pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
	m_pipeline_info.basePipelineIndex = -1;
}

void VulkanGraphicsPSO::AddRef()
{
}

void VulkanGraphicsPSO::Release()
{
}

void VulkanGraphicsPSO::InitRasterizer(const RHIGraphicsPipelineInfo& info)
{
	m_rasterizer_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	m_rasterizer_info.depthClampEnable = VK_FALSE;
	m_rasterizer_info.rasterizerDiscardEnable = VK_FALSE;
	m_rasterizer_info.polygonMode = VK_POLYGON_MODE_FILL;
	m_rasterizer_info.cullMode = VK_CULL_MODE_BACK_BIT;
	m_rasterizer_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	m_rasterizer_info.depthBiasEnable = VK_FALSE;
	m_rasterizer_info.lineWidth = 1.0f;

	m_pipeline_info.pRasterizationState = &m_rasterizer_info;
}

void VulkanGraphicsPSO::InitInputAssembly(const RHIGraphicsPipelineInfo& info)
{
	m_input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	m_input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	m_input_assembly.primitiveRestartEnable = VK_FALSE;

	VERIFY_NOT_EQUAL(info.input_assembler, nullptr);

	size_t attribute_count = 0;
	for (size_t i = 0; i < m_binding_descs.size(); ++i)
	{
		attribute_count += info.input_assembler->primitive_buffers[i]->attributes_count;
	}
	m_attribute_descs.reserve(attribute_count);

	m_binding_descs.resize(info.input_assembler->buffers_count);
	for (uint32_t i = 0; i < uint32_t(m_binding_descs.size()); ++i)
	{
		m_binding_descs[i].binding = i;
		m_binding_descs[i].inputRate = VulkanRHI::GetVkVertexInputRate(info.input_assembler->frequencies[i]);

		VERIFY_NOT_EQUAL(info.input_assembler->primitive_buffers[i], nullptr);

		const RHIPrimitiveBufferLayout& buffer_layout = *info.input_assembler->primitive_buffers[i];

		for (uint32_t j = 0; j < uint32_t(buffer_layout.attributes_count); ++j)
		{
			uint32_t location = uint32_t(m_attribute_descs.size());
			VkVertexInputAttributeDescription& vk_attr_desc = m_attribute_descs.emplace_back();
			vk_attr_desc.binding = i;
			vk_attr_desc.format = VulkanRHI::GetVkFormat(buffer_layout.attributes[j].format);
			vk_attr_desc.location = location;
			vk_attr_desc.offset = buffer_layout.attributes[j].offset;
		}

		m_binding_descs[i].stride = uint32_t(buffer_layout.stride);
	}

	m_vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	m_vertex_input_info.vertexBindingDescriptionCount = uint32_t(info.input_assembler->buffers_count);
	m_vertex_input_info.pVertexBindingDescriptions = m_binding_descs.data();
	m_vertex_input_info.vertexAttributeDescriptionCount = uint32_t(m_attribute_descs.size());
	m_vertex_input_info.pVertexAttributeDescriptions = m_attribute_descs.data();

	m_pipeline_info.pVertexInputState = &m_vertex_input_info;
	m_pipeline_info.pInputAssemblyState = &m_input_assembly;
}

void VulkanGraphicsPSO::InitDynamicState(const RHIGraphicsPipelineInfo& info)
{
	m_dynamic_states =
	{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	m_dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	m_dynamic_state_info.dynamicStateCount = uint32_t(m_dynamic_states.size());
	m_dynamic_state_info.pDynamicStates = m_dynamic_states.data();

	m_pipeline_info.pDynamicState = &m_dynamic_state_info;

	m_viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	m_viewport_info.viewportCount = uint32_t(info.rts_count);
	m_viewport_info.pViewports = nullptr;
	m_viewport_info.scissorCount = uint32_t(info.rts_count);
	m_viewport_info.pScissors = nullptr;

	m_pipeline_info.pViewportState = &m_viewport_info;
}

void VulkanGraphicsPSO::InitMultisampling(const RHIGraphicsPipelineInfo& info)
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

void VulkanGraphicsPSO::InitColorBlending(const RHIGraphicsPipelineInfo& info)
{
	auto& attachment = m_color_blend_attachments.emplace_back();
	attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	attachment.blendEnable = VK_FALSE;

	m_color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	m_color_blending.logicOpEnable = VK_FALSE;
	m_color_blending.attachmentCount = uint32_t(m_color_blend_attachments.size());
	m_color_blending.pAttachments = m_color_blend_attachments.data();

	m_pipeline_info.pColorBlendState = &m_color_blending;
}

void VulkanGraphicsPSO::InitDynamicRendering(const RHIGraphicsPipelineInfo& info)
{
	m_pipeline_dynamic_rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

	m_color_attachment_formats.resize(info.rts_count);
	for (size_t i = 0; i < info.rts_count; ++i)
	{
		m_color_attachment_formats[i] = VulkanRHI::GetVkFormat(info.rt_info[i].format);
	}

	m_pipeline_dynamic_rendering_info.colorAttachmentCount = uint32_t(m_color_attachment_formats.size());	
	m_pipeline_dynamic_rendering_info.pColorAttachmentFormats = m_color_attachment_formats.data();

	m_pipeline_info.pNext = &m_pipeline_dynamic_rendering_info;
}
