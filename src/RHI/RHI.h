#pragma once

#include <memory>

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
};


template<typename T>
using UniquePtrWithDeleter = std::unique_ptr<T, void(*)(T*)>;

using RHIPtr = UniquePtrWithDeleter<RHI>;
