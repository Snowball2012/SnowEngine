# Third-party dependencies

The solution expects these dependencies under `3rdparty/` (the repository's
`3rdparty` path is mapped to the shared third-party directory). Versions are
recorded from project paths or vendor files; `unknown/unpinned` means the
repository does not identify a version.

| Library | Version | Required path relative to `3rdparty/` |
| --- | --- | --- |
| Boost | 1.67.0 | `boost_1_67_0/` |
| DirectX Shader Compiler (DXC) | unknown/unpinned | `dxc/` |
| DirectX Tool Kit for DirectX 12 | unknown/unpinned | `dxtk12/` |
| Dear ImGui | 1.89.4 WIP (`IMGUI_VERSION_NUM` 18932) | `imgui/` |
| ImGuiFileDialog | 0.6.6 | `ImGuiFileDialog/` |
| GLM | Vulkan SDK bundled version | `VulkanSDK/1.3.216.0/Include/glm/` |
| RapidJSON | 1.1.0 | `rapidjson/` |
| SDL2 | Vulkan SDK bundled version | `VulkanSDK/1.3.216.0/` |
| Vulkan SDK | 1.3.216.0 | `VulkanSDK/1.3.216.0/` |
| Vulkan Memory Allocator | 3.0.1 | `VulkanSDK/1.3.216.0/Include/vma/` |
| stb | unknown snapshot (`STBI_VERSION` 1) | `stb/` |
| TinyEXR | unknown/unpinned | `tinyexr/` |
| tinyobjloader | unknown/unpinned | `tinyobjloader/` |
| ufbx | 0.18.2 | `ufbx/` |
| bcdec | 0.98 | `bcdec/` |

The solution also contains project-local copies of stb headers for
`RHITestApp`; those are not installed through the shared dependency path.
Windows SDK components such as Direct3D 12, DXGI, DirectXMath, and WRL are
provided by the Visual Studio 2022/Windows SDK installation rather than the
repository `3rdparty/` tree. Runtime libraries such as `SDL2.lib`,
`vulkan-1.lib`, and `dxcompiler.lib`
must be available in the corresponding SDK/library locations configured in
the Visual Studio project files.
