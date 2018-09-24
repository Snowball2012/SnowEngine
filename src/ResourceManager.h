#pragma once

// Resource management
// Responsibilities:
// 1. Mip streaming for 2D textures (reserved resources)
// 2. CPU -> GPU upload (Async / sync)
// 3. Resource availability tracking
// 4. Transient resource management (sets of non-overlapping transient resources -> sets of overlapping resources, placed optimally in a heap)
// 5. Async memory defragmentation

// This class does NOT manage resource views in any way.
// However, a resource is persistent after allocation,
// so you can safely create any descriptor you want and
// be sure it will stay valid until resource is destroyed

// Transient resources must only be created and destroyed in bundles.

class ResourceManager
{
public:
private:
};