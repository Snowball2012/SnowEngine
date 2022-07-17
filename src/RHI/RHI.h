#pragma once

#include <memory>

class RHI
{
public:
	virtual ~RHI() {};

	// temp functions, to be removed
	virtual void* GetNativeInstance() const { return nullptr; }
};


template<typename T>
using UniquePtrWithDeleter = std::unique_ptr<T, void(*)(T*)>;

using RHIPtr = UniquePtrWithDeleter<RHI>;
