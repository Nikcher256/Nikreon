# Dependencies

This project uses `vcpkg` manifest mode through `vcpkg.json`.

Core dependencies:

- Vulkan headers and loader
- volk for Vulkan function loading when the Vulkan SDK loader is not used directly
- GLFW for window/input bootstrap
- GLM for math
- stb for image loading helpers
- tinygltf for GLB/glTF loading
- FreeType for text rendering
- miniaudio for simple embedded audio
- OpenAL Soft for a dedicated positional 3D audio backend option
- EnTT for entity/component architecture
- spdlog for logging
- nlohmann-json for scene/config serialization
- Jolt Physics for physics preparation

The local `external/vcpkg` checkout is ignored by git. Recreate it with:

```sh
make deps
```

See `docs/BUILDING.md` for Windows, Linux, and macOS build instructions.

On Windows, install Visual Studio Build Tools or Visual Studio with the C++ workload before building. For Vulkan development, install the Vulkan SDK when possible. This repo also includes `volk` so the engine can use dynamic Vulkan function loading instead of depending on vcpkg building `vulkan-loader`.

`vulkan-loader` is intentionally not in the manifest because some Windows Device Guard policies block one of the temporary executables used by that port during build.
