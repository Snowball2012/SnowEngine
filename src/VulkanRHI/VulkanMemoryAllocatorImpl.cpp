#define VMA_IMPLEMENTATION

#define VMA_DEBUG_LOG(format, ...) SE_LOG( VulkanMemoryAllocator, Warning, format, __VA_ARGS__ )

#include "StdAfx.h"

#include <vma/vk_mem_alloc.h>