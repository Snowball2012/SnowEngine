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
	virtual class SwapChain* CreateSwapChain(const struct SwapChainCreateInfo& create_info) { return nullptr; }
	virtual SwapChain* GetMainSwapChain() { return nullptr; }

	virtual class Semaphore* CreateGPUSemaphore() { return nullptr; }

	struct PresentInfo
	{
		class Semaphore** wait_semaphores = nullptr;
		size_t semaphore_count = 0;
	};
	virtual void Present(SwapChain& swap_chain, const PresentInfo& info) = 0;

	virtual void WaitIdle() = 0;
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

class SwapChain : public RHIObject
{
public:
	virtual ~SwapChain() {}

	virtual glm::uvec2 GetExtent() const = 0;

	virtual void AcquireNextImage(class Semaphore* semaphore_to_signal, bool& out_recreated) = 0;

	virtual void Recreate() = 0;

	// temp, for PSO
	virtual void* GetNativeFormat() const = 0;

	virtual void* GetNativeImage() const = 0;
	virtual void* GetNativeImageView() const = 0;
};

class Semaphore : public RHIObject
{
public:
	virtual ~Semaphore() {}

	virtual void* GetNativeSemaphore() const = 0;
};

template<typename T>
using UniquePtrWithDeleter = std::unique_ptr<T, void(*)(T*)>;

using RHIPtr = UniquePtrWithDeleter<RHI>;
