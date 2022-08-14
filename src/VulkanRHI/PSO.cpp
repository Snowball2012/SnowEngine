#include "StdAfx.h"

#include "PSO.h"

#include "VulkanRHIImpl.h"

VulkanGraphicsPSO::VulkanGraphicsPSO(VulkanRHI* rhi, const RHIGraphicsPipelineInfo& info)
	: m_rhi(rhi), m_vs(static_cast<Shader*>(info.vs)), m_ps(static_cast<Shader*>(info.ps))
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
}

void VulkanGraphicsPSO::AddRef()
{
}

void VulkanGraphicsPSO::Release()
{
}
