#pragma once

#include <memory>
#include <boost/intrusive_ptr.hpp>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

class RHI
{
public:
	virtual ~RHI() {};

	// temp functions, to be removed
	virtual void* GetNativePhysDevice() const { return nullptr; }
	virtual void* GetNativeSurface() const { return nullptr; }
	virtual void* GetNativeDevice() const { return nullptr; }
	virtual void* GetNativeGraphicsQueue() const { return nullptr; }
	virtual void* GetNativeCommandPool() const { return nullptr; }

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
		Compute = 1
	};

	virtual class RHICommandList* GetCommandList(QueueType type) = 0;

	// command lists here must belong to the same QueueType
	struct SubmitInfo
	{
		size_t cmd_list_count = 0;
		class RHICommandList** cmd_lists = nullptr;
		class RHISemaphore* semaphore_to_signal = nullptr; // optional
	};
	virtual void SubmitCommandLists(const SubmitInfo& info) = 0;
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

	// temp, for PSO
	virtual void* GetNativeFormat() const = 0;

	virtual void* GetNativeImage() const = 0;
	virtual void* GetNativeImageView() const = 0;
};

class RHISemaphore : public RHIObject
{
public:
	virtual ~RHISemaphore() {}

	virtual void* GetNativeSemaphore() const = 0;
};

class RHICommandList
{
public:
	virtual ~RHICommandList() {}

	virtual void* GetNativeCmdList() const = 0;
};

template<typename T>
using UniquePtrWithDeleter = std::unique_ptr<T, void(*)(T*)>;

using RHIPtr = UniquePtrWithDeleter<RHI>;
