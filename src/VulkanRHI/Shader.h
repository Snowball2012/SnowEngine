#pragma once

#include "StdAfx.h"

#include <RHI/RHI.h>

struct ShaderDefine
{
    std::string name;
    std::string value;
};

class Shader : public RHIShader
{
    GENERATE_RHI_OBJECT_BODY()

private:
    ComPtr<struct IDxcBlob> m_bytecode_blob = nullptr;

    std::wstring m_filename;
    std::string m_entrypoint;
    RHI::ShaderFrequency m_frequency;
    std::vector<ShaderDefine> m_defines;

    VkPipelineShaderStageCreateInfo m_vk_stage = {};

public:

    Shader(VulkanRHI* rhi, std::wstring filename, RHI::ShaderFrequency frequency, std::string entry_point, std::vector<ShaderDefine> defines, ComPtr<struct IDxcBlob> bytecode);
    Shader(VulkanRHI* rhi, const class ShaderSourceFile& source, RHI::ShaderFrequency frequency, std::string entry_point, std::vector<ShaderDefine> defines = std::vector<ShaderDefine>());

    virtual ~Shader();

    const wchar_t* GetFileName() const { return m_filename.c_str(); }
    const char* GetEntryPoint() const { return m_entrypoint.c_str(); }

    const VkPipelineShaderStageCreateInfo& GetVkStageInfo() const { return m_vk_stage; }

    bool Recompile();

private:
    bool Compile(const ShaderSourceFile* source);
    void CreateVkStage();

    void CleanupShaderStage();
};

class ShaderSourceFile
{
private:
    std::wstring m_filename;
    ComPtr<struct IDxcBlobEncoding> m_blob = nullptr;

public:
    ShaderSourceFile(std::wstring filename, ComPtr<IDxcBlobEncoding> blob);

    IDxcBlobEncoding* GetNativeBlob() const { return m_blob.Get(); }
    const wchar_t* GetFilename() const { return m_filename.c_str(); }
};

class ShaderCompiler
{
private:
    static std::unique_ptr<ShaderCompiler> m_shared_instance;

    ComPtr<struct IDxcCompiler> m_dxc_compiler = nullptr;
    ComPtr<struct IDxcUtils> m_dxc_utils = nullptr;
    ComPtr<struct IDxcIncludeHandler> m_dxc_include_header = nullptr;

public:
    static ShaderCompiler* Get();

    std::unique_ptr<ShaderSourceFile> LoadSourceFile(std::wstring filename);
    ComPtr<IDxcBlob> CompileFromSource(const ShaderSourceFile& source, RHI::ShaderFrequency frequency, const std::string& entry_point, const span<const ShaderDefine>& defines);

private:
    ShaderCompiler();
};

class VulkanShaderBindingTableLayout : public RHIShaderBindingTableLayout
{
    GENERATE_RHI_OBJECT_BODY()

    VkDescriptorSetLayout m_vk_desc_set_layout = VK_NULL_HANDLE;

public:
    virtual ~VulkanShaderBindingTableLayout() override;

    VulkanShaderBindingTableLayout(VulkanRHI* rhi, const RHI::ShaderBindingTableLayoutInfo& info);

    VkDescriptorSetLayout GetVkDescriptorSetLayout() const { return m_vk_desc_set_layout; }
};
IMPLEMENT_RHI_INTERFACE(RHIShaderBindingTableLayout, VulkanShaderBindingTableLayout);

class VulkanShaderBindingLayout : public RHIShaderBindingLayout
{
    GENERATE_RHI_OBJECT_BODY()

    std::vector<RHIObjectPtr<VulkanShaderBindingTableLayout>> m_layout_infos;
    VkPipelineLayout m_vk_pipeline_layout = VK_NULL_HANDLE;
    size_t m_num_push_constants = 0;

public:
    virtual ~VulkanShaderBindingLayout() override;

    VulkanShaderBindingLayout(VulkanRHI* rhi, const RHI::ShaderBindingLayoutInfo& info);

    size_t GetPushConstantsCount() const { return m_num_push_constants; }

    VkPipelineLayout GetVkPipelineLayout() const { return m_vk_pipeline_layout; }
};
IMPLEMENT_RHI_INTERFACE(RHIShaderBindingLayout, VulkanShaderBindingLayout)

class VulkanShaderBindingTable : public RHIShaderBindingTable
{
    GENERATE_RHI_OBJECT_BODY()

    VkDescriptorSet m_vk_desc_set = VK_NULL_HANDLE;
    RHIObjectPtr<VulkanShaderBindingTableLayout> m_sbt_layout = nullptr;

    boost::container::small_vector<VkWriteDescriptorSet, 4> m_pending_writes;

    // we need this to make sure pBufferInfo and such are valid at the time of FlushBinds
    boost::container::small_vector<RHIObjectPtr<RHIObject>, 4> m_referenced_views;

public:
    virtual ~VulkanShaderBindingTable() override;

    VulkanShaderBindingTable(VulkanRHI* rhi, RHIShaderBindingTableLayout& layout);

    virtual void BindCBV(size_t range_idx, size_t idx_in_range, RHICBV& cbv) override;
    virtual void BindSRV(size_t range_idx, size_t idx_in_range, RHITextureSRV& srv) override;
    virtual void BindSampler(size_t range_idx, size_t idx_in_range, RHISampler& srv) override;

    virtual void FlushBinds() override;

    VkDescriptorSet GetVkDescriptorSet() const { return m_vk_desc_set; }

private:
    void InitWriteStruct(VkWriteDescriptorSet& write_struct, size_t range_idx, size_t idx_in_range) const;
};
IMPLEMENT_RHI_INTERFACE(RHIShaderBindingTable, VulkanShaderBindingTable)