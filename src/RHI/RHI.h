#pragma once

#include <memory>
#include <boost/intrusive_ptr.hpp>

class RHI
{
public:
	virtual ~RHI() {};

	// temp functions, to be removed
	virtual void* GetNativePhysDevice() const { return nullptr; }
	virtual void* GetNativeSurface() const { return nullptr; }
	virtual void* GetNativeDevice() const { return nullptr; }
	virtual void* GetNativeGraphicsQueue() const { return nullptr; }
	virtual void* GetNativePresentQueue() const { return nullptr; }

	// window_handle must outlive the swap chain
	virtual class SwapChain* CreateSwapChain(const struct SwapChainCreateInfo& create_info) { return nullptr; }
	virtual SwapChain* GetMainSwapChain() { return nullptr; }
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

namespace boost
{
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
}

struct SwapChainCreateInfo
{
	void* window_handle = nullptr;
	uint32_t surface_num = 0;
};

class SwapChain : public RHIObject
{
public:
	virtual ~SwapChain() {};
};

template<typename T>
using UniquePtrWithDeleter = std::unique_ptr<T, void(*)(T*)>;

using RHIPtr = UniquePtrWithDeleter<RHI>;
