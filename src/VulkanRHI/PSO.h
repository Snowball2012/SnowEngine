#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

#include "Shader.h"

class VulkanUploadBuffer;

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

    VkPipeline m_vk_pipeline = VK_NULL_HANDLE;

    RHIObjectPtr<VulkanShaderBindingLayout> m_shader_bindings = nullptr;
    RHIObjectPtr<Shader> m_vs = nullptr;
    RHIObjectPtr<Shader> m_ps = nullptr;

public:

    VulkanGraphicsPSO( VulkanRHI* rhi, const RHIGraphicsPipelineInfo& info );

    virtual ~VulkanGraphicsPSO() override;

    VkPipeline GetVkPipeline() const { return m_vk_pipeline; }
    VkPipelineLayout GetVkPipelineLayout() const { return m_shader_bindings->GetVkPipelineLayout(); }

    size_t GetPushConstantsCount() const { return m_shader_bindings->GetPushConstantsCount(); }

    bool Recompile();

private:
    void InitRasterizer( const RHIGraphicsPipelineInfo& info );
    void InitInputAssembly( const RHIGraphicsPipelineInfo& info );
    void InitDynamicState( const RHIGraphicsPipelineInfo& info );
    void InitMultisampling( const RHIGraphicsPipelineInfo& info );
    void InitColorBlending( const RHIGraphicsPipelineInfo& info );
    void InitDynamicRendering( const RHIGraphicsPipelineInfo& info );
};
IMPLEMENT_RHI_INTERFACE( RHIGraphicsPipeline, VulkanGraphicsPSO )


class VulkanRaytracingPSO : public RHIRaytracingPipeline
{
    GENERATE_RHI_OBJECT_BODY()

    VkPipeline m_vk_pipeline = VK_NULL_HANDLE;

    VkRayTracingPipelineCreateInfoKHR m_vk_pipeline_ci = {};

    bc::static_vector<VkPipelineShaderStageCreateInfo, 5> m_stages = {};
    bc::small_vector<VkRayTracingShaderGroupCreateInfoKHR, 3> m_groups = {}; // raygen, 1 miss and 1 hit group by default

    // Embed SBT into a pipeline. Use generic shader bindings mechanism to pass data in a "bindless" way.
    RHIObjectPtr<VulkanUploadBuffer> m_sbt = nullptr;
    VkStridedDeviceAddressRegionKHR m_rgs_region = {};

    RHIObjectPtr<VulkanShaderBindingLayout> m_shader_bindings = nullptr;

    RHIObjectPtr<Shader> m_rgs = nullptr;

public:

    VulkanRaytracingPSO( VulkanRHI* rhi, const RHIRaytracingPipelineInfo& info );

    virtual ~VulkanRaytracingPSO() override;

    bool Recompile();

    VkStridedDeviceAddressRegionKHR GetVkRaygenSBT() const;

private:
    void InitShaderStages();

    void InitSBT();
};
IMPLEMENT_RHI_INTERFACE( RHIRaytracingPipeline, VulkanRaytracingPSO )
