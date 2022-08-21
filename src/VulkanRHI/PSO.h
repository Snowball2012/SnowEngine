#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

#include "Shader.h"

class VulkanGraphicsPSO : public RHIGraphicsPipeline
{
	GENERATE_RHI_OBJECT_BODY()

	VkPipelineShaderStageCreateInfo m_stages[2];

	VkPipelineVertexInputStateCreateInfo m_vertex_input_info = {};
	std::vector<VkVertexInputAttributeDescription> m_attribute_descs = {};
	std::vector<VkVertexInputBindingDescription> m_binding_descs = {};
	VkPipelineInputAssemblyStateCreateInfo m_input_assembly = {};

	std::vector<VkDynamicState> m_dynamic_states = {};
	VkPipelineDynamicStateCreateInfo m_dynamic_state_info = {};

	VkPipelineRasterizationStateCreateInfo m_rasterizer_info = {};

	VkPipelineViewportStateCreateInfo m_viewport_info = {};

	VkPipelineMultisampleStateCreateInfo m_multisampling = {};

	std::vector<VkPipelineColorBlendAttachmentState> m_color_blend_attachments = {};
	VkPipelineColorBlendStateCreateInfo m_color_blending = {};

	std::vector<VkFormat> m_color_attachment_formats = {};
	VkPipelineRenderingCreateInfo m_pipeline_dynamic_rendering_info = {};

	VkGraphicsPipelineCreateInfo m_pipeline_info = {};

	RHIObjectPtr<Shader> m_vs = nullptr;
	RHIObjectPtr<Shader> m_ps = nullptr;

public:

	VulkanGraphicsPSO(VulkanRHI* rhi, const RHIGraphicsPipelineInfo& info);

	virtual void* GetNativePipelineCreateInfo() const override { return (void*)&m_pipeline_info; }

private:
	void InitRasterizer(const RHIGraphicsPipelineInfo& info);
	void InitInputAssembly(const RHIGraphicsPipelineInfo& info);
	void InitDynamicState(const RHIGraphicsPipelineInfo& info);
	void InitMultisampling(const RHIGraphicsPipelineInfo& info);
	void InitColorBlending(const RHIGraphicsPipelineInfo& info);
	void InitDynamicRendering(const RHIGraphicsPipelineInfo& info);
};