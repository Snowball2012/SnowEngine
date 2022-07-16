#pragma once

#include <memory>

class RHI
{
public:
	virtual ~RHI() {};
};


template<typename T>
using UniquePtrWithDeleter = std::unique_ptr<T, void(*)(T*)>;

using RHIPtr = UniquePtrWithDeleter<RHI>;
