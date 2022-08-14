#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

#include "Shader.h"

class VulkanGraphicsPSO : public RHIGraphicsPipeline
{
	class VulkanRHI* m_rhi = nullptr;

	VkPipelineVertexInputStateCreateInfo m_vertex_input_info = {};
	std::vector<VkVertexInputAttributeDescription> m_attribute_descs = {};
	std::vector<VkVertexInputBindingDescription> m_binding_descs = {};

	VkPipelineInputAssemblyStateCreateInfo m_input_assembly = {};

	RHIObjectPtr<Shader> m_vs = nullptr;
	RHIObjectPtr<Shader> m_ps = nullptr;

public:

	VulkanGraphicsPSO(VulkanRHI* rhi, const RHIGraphicsPipelineInfo& info);

	virtual void* GetNativeInputStateCreateInfo() const override { return (void*)&m_vertex_input_info; }
	void* GetNativeInputAssemblyCreateInfo() const override { return (void*)&m_input_assembly; }

	// Inherited via RHIGraphicsPipeline
	virtual void AddRef() override;
	virtual void Release() override;
};