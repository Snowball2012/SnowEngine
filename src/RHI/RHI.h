#pragma once

#include <memory>
#include <boost/intrusive_ptr.hpp>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

struct RHIFence
{
	uint64_t _1;
	uint64_t _2;
	uint64_t _3;
};

#ifndef IMPLEMENT_SCOPED_ENUM_FLAGS
#define IMPLEMENT_SCOPED_ENUM_FLAGS(T) \
inline constexpr T operator|(T lhs, T rhs) \
{ \
	return T( \
		std::underlying_type<T>::type(lhs) \
		| std::underlying_type<T>::type(rhs)); \
} \
inline constexpr T operator&(T lhs, T rhs) \
{ \
	return T( \
		std::underlying_type<T>::type(lhs) \
		& std::underlying_type<T>::type(rhs)); \
}
#endif

enum class RHIBufferUsageFlags : uint32_t
{
	None = 0,
	TransferSrc = 0x1,
	TransferDst = 0x2,
	VertexBuffer = 0x4,
	IndexBuffer = 0x8,
	UniformBuffer = 0x10,

	NumFlags = 5
};
IMPLEMENT_SCOPED_ENUM_FLAGS(RHIBufferUsageFlags)

enum class RHIPipelineStageFlags : uint32_t
{
	ColorAttachmentOutput = 0x00000001,
	VertexShader = 0x00000002,
	PixelShader = 0x00000004,

	AllBits = 0x00000007,

	NumFlags = 3,
};
IMPLEMENT_SCOPED_ENUM_FLAGS(RHIPipelineStageFlags)

enum class RHIShaderStageFlags : uint32_t
{
	VertexShader = 0x00000001,
	PixelShader = 0x00000002,
	
	AllBits = 0x00000003,
	
	NumFlags = 2,
};
IMPLEMENT_SCOPED_ENUM_FLAGS(RHIShaderStageFlags)

enum class RHITextureDimensions : uint8_t
{
	T1D,
	T2D,
	T3D
};

enum class RHITextureUsageFlags : uint32_t
{
	None = 0,

	TransferSrc = 0x1,
	TransferDst = 0x2,
	SRV = 0x4,
	RTV = 0x8,

	NumFlags = 4
};
IMPLEMENT_SCOPED_ENUM_FLAGS(RHITextureUsageFlags)

enum class RHIFormat : uint8_t
{
	Undefined = 0,
	R32G32_SFLOAT,
	R32G32B32_SFLOAT,
	B8G8R8A8_SRGB,
	R8G8B8A8_SRGB
};

enum class RHIShaderBindingType : uint8_t
{
	ConstantBuffer = 0,
	TextureSRV,
	Sampler
};

enum class RHITextureLayout : uint8_t
{
	Undefined = 0,
	TransferDst
};

class RHI
{
public:
	virtual ~RHI() {};

	// temp functions, to be removed
	virtual void* GetNativeDevice() const { return nullptr; }
	virtual void* GetNativeDescPool() const { return nullptr; }

	// window_handle must outlive the swap chain
	virtual class RHISwapChain* CreateSwapChain(const struct SwapChainCreateInfo& create_info) { return nullptr; }
	virtual RHISwapChain* GetMainSwapChain() { return nullptr; }

	virtual class RHISemaphore* CreateGPUSemaphore() { return nullptr; }

	struct PresentInfo
	{
		class RHISemaphore** wait_semaphores = nullptr;
		size_t semaphore_count = 0;
	};
	virtual void Present(RHISwapChain& swap_chain, const PresentInfo& info) = 0;

	virtual void WaitIdle() = 0;

	enum class QueueType : uint8_t
	{
		Graphics = 0,
		Count
	};

	virtual class RHICommandList* GetCommandList(QueueType type) { return nullptr; };

	// command lists here must belong to the same QueueType
	struct SubmitInfo
	{
		size_t cmd_list_count = 0;
		class RHICommandList* const* cmd_lists = nullptr;
		size_t wait_semaphore_count = 0;
		class RHISemaphore* const* semaphores_to_wait = nullptr;
		const RHIPipelineStageFlags* stages_to_wait = nullptr;
		class RHISemaphore* semaphore_to_signal = nullptr; // optional
	};
	virtual RHIFence SubmitCommandLists(const SubmitInfo& info) = 0;

	virtual void WaitForFenceCompletion(const RHIFence& fence) = 0;

	enum class ShaderFrequency : uint8_t
	{
		Vertex = 0,
		Pixel,
		Compute,
		Raytracing,
		Count
	};

	struct ShaderDefine
	{
		const char* name = nullptr;
		const char* value = nullptr;
	};
	struct ShaderCreateInfo
	{
		const wchar_t* filename = nullptr;
		ShaderFrequency frequency = ShaderFrequency::Vertex;
		const char* entry_point = nullptr;
		const ShaderDefine* defines = nullptr;
		size_t define_count = 0;
	};
	// Shader stuff
	virtual class RHIShader* CreateShader(const ShaderCreateInfo& shader_info) { return nullptr; }


	struct ShaderViewRange
	{
		RHIShaderBindingType type = RHIShaderBindingType::ConstantBuffer;
		int32_t count = 1; // negative means unbound bindless
		RHIShaderStageFlags stages = RHIShaderStageFlags::AllBits; // used only in CreateShaderBindingLayout
	};

	struct ShaderBindingTableLayoutInfo
	{
		const ShaderViewRange* ranges = nullptr;
		size_t range_count = 0;
	};
	virtual class RHIShaderBindingTableLayout* CreateShaderBindingTableLayout(const ShaderBindingTableLayoutInfo& info) { return nullptr; }

	virtual class RHIShaderBindingTable* CreateShaderBindingTable(RHIShaderBindingTableLayout& layout) { return nullptr; }

	struct ShaderBindingLayoutInfo
	{
		RHIShaderBindingTableLayout* const* tables = nullptr;
		size_t table_count = 0;
	};

	virtual class RHIShaderBindingLayout* CreateShaderBindingLayout(const ShaderBindingLayoutInfo& layout_info) { return nullptr; }
	
	virtual class RHIGraphicsPipeline* CreatePSO(const struct RHIGraphicsPipelineInfo& pso_info) { return nullptr; }

	struct BufferInfo
	{
		uint64_t size = 0;
		RHIBufferUsageFlags usage = RHIBufferUsageFlags::None;
	};
	virtual class RHIUploadBuffer* CreateUploadBuffer(const BufferInfo& info) { return nullptr; }
	virtual class RHIBuffer* CreateDeviceBuffer(const BufferInfo& info) { return nullptr; }

	struct TextureInfo
	{
		RHITextureDimensions dimensions = RHITextureDimensions::T2D;
		RHIFormat format = RHIFormat::Undefined;

		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t depth = 0;

		uint32_t mips = 0;
		uint32_t array_layers = 0;

		RHITextureUsageFlags usage = RHITextureUsageFlags::None;

		RHITextureLayout initial_layout = RHITextureLayout::Undefined;

		RHI::QueueType initial_queue = RHI::QueueType::Graphics;
	};
	virtual class RHITexture* CreateTexture(const TextureInfo& info) { return nullptr; }

	struct CBVInfo
	{
		RHIBuffer* buffer = nullptr;
	};
	virtual class RHICBV* CreateCBV(const CBVInfo& info) { return nullptr; }

	struct TextureSRVInfo
	{
		RHITexture* texture = nullptr;
	};
	virtual class RHITextureSRV* CreateSRV(const TextureSRVInfo& info) { return nullptr; }

	struct SamplerInfo
	{
		float mip_bias = 0.0f;
		bool enable_anisotropy = false;
	};
	virtual class RHISampler* CreateSampler(const SamplerInfo& info) { return nullptr; }
};

class RHIObject
{
public:
	virtual void AddRef() = 0;
	virtual void Release() = 0;

	virtual ~RHIObject() {}
};

// helpers for reference counting
template<typename T>
using RHIObjectPtr = boost::intrusive_ptr<T>;

inline void intrusive_ptr_add_ref(RHIObject* p)
{
	if (p)
		p->AddRef();
}
inline void intrusive_ptr_release(RHIObject* p)
{
	if (p)
		p->Release();
}

struct SwapChainCreateInfo
{
	void* window_handle = nullptr;
	uint32_t surface_num = 0;
};

class RHISwapChain : public RHIObject
{
public:
	virtual ~RHISwapChain() {}

	virtual glm::uvec2 GetExtent() const = 0;

	virtual void AcquireNextImage(class RHISemaphore* semaphore_to_signal, bool& out_recreated) = 0;

	virtual void Recreate() = 0;

	virtual RHIFormat GetFormat() const { return RHIFormat::Undefined; }

	virtual const RHITexture* GetTexture() const { return nullptr; }

	virtual void* GetNativeImageView() const = 0;
};

class RHISemaphore : public RHIObject
{
public:
	virtual ~RHISemaphore() {}

	virtual void* GetNativeSemaphore() const = 0;
};

enum class RHIIndexBufferType
{
	UInt16,
	UInt32
};

class RHICommandList
{
public:
	virtual ~RHICommandList() {}

	virtual RHI::QueueType GetType() const = 0;

	virtual void Begin() = 0;
	virtual void End() = 0;

	struct CopyRegion
	{
		size_t src_offset = 0;
		size_t dst_offset = 0;
		size_t size = 0;
	};
	virtual void CopyBuffer(RHIBuffer& src, RHIBuffer& dst, size_t region_count, CopyRegion* regions) {}

	virtual void DrawIndexed(
		uint32_t index_count,
		uint32_t instance_count,
		uint32_t first_index,
		int32_t vertex_offset,
		uint32_t first_instance) {}

	virtual void SetPSO(RHIGraphicsPipeline& pso) {}

	virtual void BindTable(size_t slot_idx, RHIShaderBindingTable& table) {}

	virtual void SetIndexBuffer(RHIBuffer& index_buf, RHIIndexBufferType type, size_t offset) {};

	virtual void BeginPass() {}
	virtual void EndPass() {}

	virtual void* GetNativeCmdList() const = 0;
};

class RHIShader : public RHIObject
{
public:
	virtual ~RHIShader() {}
};

class RHIGraphicsPipeline : public RHIObject
{
public:
	virtual ~RHIGraphicsPipeline() {}
};
using RHIGraphicsPipelinePtr = RHIObjectPtr<RHIGraphicsPipeline>;

class RHIShaderBindingLayout : public RHIObject
{
public:
	virtual ~RHIShaderBindingLayout() {}

	// TEMP
	virtual void* GetNativePipelineLayout() const { return nullptr; }
};
using RHIShaderBindingLayoutPtr = RHIObjectPtr<RHIShaderBindingLayout>;

class RHIShaderBindingTable : public RHIObject
{
public:
	virtual void BindCBV(size_t range_idx, size_t idx_in_range, RHICBV& cbv) {}
	virtual void BindSRV(size_t range_idx, size_t idx_in_range, RHITextureSRV& srv) {}
	virtual void BindSampler(size_t range_idx, size_t idx_in_range, RHISampler& sampler) {}

	virtual void FlushBinds() {}

	virtual ~RHIShaderBindingTable() {}
};
using RHIShaderBindingTablePtr = RHIObjectPtr<RHIShaderBindingTable>;

class RHIShaderBindingTableLayout : public RHIObject
{
public:
	virtual ~RHIShaderBindingTableLayout() {}
	virtual void* GetNativeDescLayout() const { return nullptr; }
};
using RHIShaderBindingTableLayoutPtr = RHIObjectPtr<RHIShaderBindingTableLayout>;

enum class RHIPrimitiveFrequency : uint8_t
{
	PerVertex = 0,
	PerInstance = 1
};

struct RHIPrimitiveAttributeInfo
{
	const char* semantic = nullptr;
	RHIFormat format = RHIFormat::Undefined;
	uint32_t offset = 0;
};

struct RHIPrimitiveBufferLayout
{
	const RHIPrimitiveAttributeInfo* attributes = nullptr;
	size_t attributes_count = 0;
	size_t stride = 0;
};

struct RHIInputAssemblerInfo
{
	const RHIPrimitiveBufferLayout** primitive_buffers = nullptr;
	const RHIPrimitiveFrequency* frequencies = nullptr;
	size_t buffers_count = 0;
};

struct RHIPipelineRTInfo
{
	RHIFormat format = RHIFormat::Undefined;
};

struct RHIGraphicsPipelineInfo
{
	const RHIInputAssemblerInfo* input_assembler = nullptr;

	RHIShader* vs = nullptr;
	RHIShader* ps = nullptr;

	RHIShaderBindingLayout* binding_layout = nullptr;

	const RHIPipelineRTInfo* rt_info = nullptr;
	size_t rts_count = 0;
};

class RHIBuffer : public RHIObject
{
public:
	virtual ~RHIBuffer() override {}

	virtual void* GetNativeBuffer() const { return nullptr; }
};
using RHIBufferPtr = RHIObjectPtr<RHIBuffer>;

class RHIUploadBuffer : public RHIObject
{
public:
	virtual ~RHIUploadBuffer() override {}

	// For rhi needs
	virtual RHIBuffer* GetBuffer() { return nullptr; }
	virtual const RHIBuffer* GetBuffer() const { return nullptr; }

	virtual void WriteBytes(const void* src, size_t size, size_t offset = 0) {}
};
using RHIUploadBufferPtr = RHIObjectPtr<RHIUploadBuffer>;

class RHITexture : public RHIObject
{
public:
	virtual ~RHITexture() override {}

	virtual void* GetNativeTexture() const { return nullptr; }
};
using RHITexturePtr = RHIObjectPtr<RHITexture>;

// Views
class RHICBV : public RHIObject
{
public:
	virtual ~RHICBV() override {}
};
using RHICBVPtr = RHIObjectPtr<RHICBV>;

class RHITextureSRV : public RHIObject
{
public:
	virtual ~RHITextureSRV() override {}
};
using RHITextureSRVPtr = RHIObjectPtr<RHITextureSRV>;

class RHISampler : public RHIObject
{
public:
	virtual ~RHISampler() override {}
};
using RHISamplerPtr = RHIObjectPtr<RHISampler>;

template<typename T>
using UniquePtrWithDeleter = std::unique_ptr<T, void(*)(T*)>;

using RHIPtr = UniquePtrWithDeleter<RHI>;
