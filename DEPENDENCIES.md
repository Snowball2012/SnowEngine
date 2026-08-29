# Third-party dependencies

The solution expects these dependencies under `3rdparty/` (the repository's
`3rdparty` path is mapped to the shared third-party directory). Versions are
recorded from project paths or vendor files; `unknown/unpinned` means the
repository does not identify a version.

| Library | Version | Required path relative to `3rdparty/` |
| --- | --- | --- |
| Boost | 1.67.0 | `boost_1_67_0/` |
| DirectX Shader Compiler (DXC) | unknown/unpinned | `dxc/` |
| Dear ImGui | unknown/unpinned | `imgui/` |
| GLM | unknown/unpinned | `glm/` |
| RapidJSON | unknown/unpinned | `rapidjson/` |
| SDL2 | unknown/unpinned (headers and libraries supplied with the Vulkan SDK layout) | `VulkanSDK/1.3.216.0/` |
| Vulkan SDK | 1.3.216.0 | `VulkanSDK/1.3.216.0/` |
| Vulkan Memory Allocator | unknown/unpinned | `VulkanMemoryAllocator/` |
| stb | unknown/unpinned | `stb/` |
| ufbx | unknown/unpinned | `ufbx/` |
| bcdec | unknown/unpinned | `bcdec/` |

The solution also contains project-local copies of stb headers for
`RHITestApp`; those are not installed through the shared dependency path.
Runtime libraries such as `SDL2.lib`, `vulkan-1.lib`, and `dxcompiler.lib`
must be available in the corresponding SDK/library locations configured in
the Visual Studio project files.
