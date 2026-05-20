# Modern Vulkan Game Engine Upgrade Plan

This document is the working roadmap for upgrading the C++ Vulkan engine into a modular modern game engine. It is intentionally phased: each phase should compile, preserve existing working behavior where possible, and avoid rewriting unrelated systems.

The main target is a renderer and engine architecture that can load Blender-exported GLB/glTF assets with proper materials, textures, lighting, transparency, shadows, and later animation, while also supporting a real GPU-driven 2D/UI/HUD pipeline.

## Current Starting Point

The repository now has the project bootstrap in place:

- Git repository initialized.
- CMake project created.
- `Makefile` and platform scripts added for Windows, Linux, and macOS.
- `vcpkg.json` dependency manifest added.
- Core external libraries selected: GLFW, GLM, Vulkan headers, volk, stb, tinygltf, FreeType, OpenAL Soft, miniaudio, EnTT, spdlog, nlohmann-json, and Jolt Physics.
- Minimal `src/main.cpp` builds and runs.

The next work should not jump directly into complex PBR rendering. The safer path is to build a small engine skeleton first, add minimal Vulkan ownership, build an engine-native 2D/UI layer for editor controls, then add the main viewport and 3D renderer.

## Guiding Rules

- Keep existing working code unless a change is needed for the current phase.
- Do not build one huge renderer file. Split responsibility into small engine modules.
- Every phase should build before moving on.
- Prefer glTF/GLB as the primary Blender import path.
- HUD, menus, text, sprites, debug overlays, and editor overlays are not random 3D objects. They render through the 2D/UI/debug systems after the 3D world and post-processing.
- Do not use ImGui. Editor UI, game HUD, menus, debug panels, and tools should be engine-native and GPU-rendered through the engine UI stack.
- Avoid recreating Vulkan pipelines, descriptor sets, textures, static buffers, or model resources every frame.
- Organize shaders by purpose and material style so realistic PBR, unlit, toon, debug, sprite, text, shadow, skybox, and post-process passes can coexist.

## Target Render Order

1. Shadow pass
2. 3D opaque world pass
3. Skybox pass
4. Transparent 3D pass
5. Post-processing pass
6. Screen-space HUD/UI pass
7. Editor/debug overlay pass
8. Present

HUD/UI normally renders after post-processing so text, crosshair, menus, and health bars are not blurred by bloom, depth of field, or tone mapping artifacts.

## Target Module Layout

The exact folders can follow the current codebase once implementation starts, but the ownership boundaries should remain close to this:

```text
Engine/
  Core/
    Application
    Input
    Time
    Logging
  Renderer/
    Renderer
    RenderGraph or RenderPipeline
    Renderer3D
    Renderer2D
    TextRenderer
    UIRenderer
    DebugRenderer
    ShadowRenderer
    SkyboxRenderer
    PostProcessRenderer
  Resources/
    ResourceManager
    TextureManager
    MaterialManager
    MeshManager
    ShaderManager
    FontManager
    AudioManager
  Assets/
    ModelLoader
    TextureLoader
    FontLoader
  Scene/
    Scene
    Entity or GameObject
    Component
    Transform
    Camera
    MeshRendererComponent
    LightComponent
    AudioSourceComponent
    AudioListenerComponent
    RigidbodyComponent
    ColliderComponent
    ScriptComponent
  Physics/
    PhysicsSystem
    Raycast
    Colliders
  Audio/
    AudioSystem
  Scripting/
    Script
    NativeScriptSystem
  Editor/
    EditorLayer
    EditorUI
    EditorPanels
    Gizmos
    Picking
    Selection
```

## Phase 0: Project Bootstrap

Status: complete.

Completed:

- Add CMake project.
- Add vcpkg manifest.
- Add Makefile and platform scripts.
- Add build documentation.
- Verify the bootstrap executable builds and runs on Windows.

Build gate:

- `scripts/build.ps1` succeeds.
- `scripts/run.ps1` succeeds.
- `make deps`, `make build`, and `make run` are documented for all platforms.

## Phase 1: Engine App Shell

Status: complete.

Goal: replace the single-file bootstrap with a small engine/editor application loop that still does not own complex rendering.

Tasks:

- Create basic folders under `src/Engine`.
- Add `Engine::Application`.
- Add `Engine::Window` using GLFW.
- Add `Engine::Time` for delta time.
- Add `Engine::Input` wrapper for keyboard/mouse polling.
- Add `Engine::Log` wrapper around spdlog.
- Move startup/shutdown logic out of `main.cpp`.
- Keep the app loop simple: create window, poll events, update, render stub, shutdown.
- Prepare an `EditorLayer` placeholder, but do not render UI yet.

Build gate:

- Window opens and closes cleanly.
- Escape or window close exits the app.
- Logs show startup/shutdown.
- No Vulkan device/swapchain required yet.

Completed note:

- Added `Application`, `Window`, `Time`, `Input`, and `Log`.
- `main.cpp` now creates and runs `Engine::Application`.
- Added `--smoke-test` mode for one-frame startup/shutdown verification.
- Verified with `scripts/build.ps1` and `build/Debug/NikreonEngine.exe --smoke-test`.

## Phase 2: Minimal Vulkan Frame

Goal: create enough Vulkan ownership to draw to the window before building editor UI.

Tasks:

- Add `Renderer/Vulkan/VulkanContext`.
- Initialize volk.
- Create Vulkan instance.
- Create GLFW Vulkan surface.
- Select physical device.
- Create logical device and queues.
- Add debug messenger in debug builds if validation layers are available.
- Add `Swapchain` wrapper.
- Add command pool and command buffers.
- Add per-frame synchronization objects.
- Handle window resize with safe swapchain recreation.

Build gate:

- App opens a Vulkan-backed window.
- Clear screen with a solid color.
- Resize works without crashing.
- Shutdown releases Vulkan resources cleanly.

## Phase 3: Minimal Renderer2D Foundation

Goal: build the first engine-native rendering path for editor UI primitives.

Scope:

- This is not the full game/UI renderer yet.
- Start with the smallest useful Vulkan 2D pipeline.
- Colored quads are enough for the first build gate.
- Textured quads, batching limits, atlases, and text come next.

Tasks:

- Add `Renderer2D`.
- Add a simple quad pipeline.
- Add orthographic screen-space projection.
- Add dynamic vertex/index buffer or a simple first-pass upload path.
- Draw filled rectangles.
- Draw rectangle outlines if cheap.
- Add basic z/layer ordering.
- Add alpha blending.
- Add `begin`, `drawQuad`, `drawRect`, and `end`.

Build gate:

- Engine can draw multiple colored quads on screen.
- UI-like rectangles render in screen coordinates.
- Resize updates projection correctly.
- No one-draw-call-per-widget architecture is baked in.

## Phase 4: Engine-Native Editor UI Shell

Goal: create editor controls first, using the engine's own 2D/UI renderer.

Important:

- This is the first real UI users can play with.
- Keep UI logic separate from `Renderer2D`.
- `Renderer2D` draws primitives only.
- `EditorUI` handles layout, state, input, and widgets.

Tasks:

- Add `UIRenderer` or `UIContext`.
- Add `Editor/EditorLayer`.
- Add `Editor/EditorUI`.
- Add immediate-mode or retained-mode UI decision note after a small prototype.
- Add panels/windows drawn as quads.
- Add buttons.
- Add checkboxes/toggles.
- Add sliders.
- Add text input boxes, even if text rendering starts with placeholder rectangles/caret.
- Add simple layout: rows, columns, padding, margin.
- Add hover, active, focused, clicked states.
- Add basic theme colors.
- Add top toolbar with Play, Pause, Stop buttons.
- Add left hierarchy placeholder panel.
- Add right inspector placeholder panel.
- Add bottom console/log placeholder panel.
- Add center viewport placeholder panel.

Build gate:

- User can click buttons.
- User can toggle checkboxes.
- User can drag sliders.
- User can type into an input box once text support exists, or see a clear placeholder until text is implemented.
- Panels resize with the window.
- Center viewport placeholder exists but does not need to render 3D yet.

## Phase 5: Text Rendering

Goal: add font atlas text so editor UI, HUD, labels, and debug text can show real strings.

Required features:

- Load TTF/OTF fonts if possible.
- Generate and cache font atlases.
- Cache glyph metrics.
- Draw screen-space text.
- Support font size, color, opacity, alignment, line spacing, and text measurement.
- Prepare word wrapping and MSDF support later.

Suggested API:

```cpp
textRenderer.loadFont("default", "assets/fonts/Roboto.ttf", size);
textRenderer.drawText("HP: 100", position, size, color);
glm::vec2 size = textRenderer.measureText("Inventory");
```

Recommended libraries:

- FreeType for robust font loading, or
- `stb_truetype` for a simpler first version.

Build gate:

- Text renders as glyph quads.
- Glyph metrics and atlas texture are cached.
- Buttons, labels, input fields, console, and debug text can display strings.

## Phase 6: Editor Viewport Integration

Goal: make the editor viewport panel the place where the engine will render the game/editor scene.

Tasks:

- Add a viewport render target abstraction.
- Render the Vulkan clear color or test scene into the viewport panel.
- Track viewport size and mouse position.
- Add viewport focus/hover state.
- Add editor camera placeholder controls.
- Prepare picking coordinates from viewport-local mouse position.
- Keep menu/panels independent from viewport rendering.

Build gate:

- Main editor window has UI panels plus a working viewport panel.
- Viewport resizes without breaking the swapchain/render target.
- Editor UI remains interactive while the viewport updates.

## Phase 7: Clean Render Architecture

Goal: introduce the architecture and render pass order without breaking existing rendering.

Tasks:

- Add or prepare the main renderer orchestration layer.
- Add empty or minimal modules for `Renderer3D`, `Renderer2D`, `TextRenderer`, `UIRenderer`, `DebugRenderer`, `ShadowRenderer`, `SkyboxRenderer`, and `PostProcessRenderer`.
- Define a frame render sequence matching the target render order.
- Add clear interfaces for per-frame begin/end, resize, resource cleanup, and command buffer recording.
- Prepare placeholders for screen-space UI, world-space UI, billboards, editor overlays, and debug labels.

Build gate:

- Existing editor UI and viewport placeholder still render.
- New modules compile even if most are placeholders.
- No Vulkan resources are leaked or recreated per frame unnecessarily.

## Phase 8: Full Renderer2D, Sprites, and Quads

Goal: expand the minimal 2D renderer into a real batched GPU 2D renderer.

Required features:

- Batched colored quads
- Batched textured quads
- Sprites and sprite sheets
- Texture atlas support
- UV coordinates
- Rotation, scaling, tint color, opacity
- Z/layer sorting
- Orthographic projection
- Screen-space coordinates
- Line rendering
- Rectangle outlines and filled rectangles
- Simple circles if practical
- Scissor/clipping rectangles for UI panels

Suggested API:

```cpp
renderer2D.begin(projection);
renderer2D.drawQuad(position, size, color);
renderer2D.drawSprite(texture, position, size, uv, tint);
renderer2D.drawRotatedSprite(texture, position, size, rotation, uv, tint);
renderer2D.drawLine(start, end, color, thickness);
renderer2D.drawRect(position, size, color);
renderer2D.end();
```

Implementation notes:

- Use dynamic vertex/index buffers or a ring buffer.
- Use one shared quad index pattern.
- Flush when max quads are reached.
- Flush when texture slots are full.
- Group by texture where reasonable.
- Use alpha blending.
- Do not issue one draw call per sprite.

Core vertex:

```cpp
struct QuadVertex {
    glm::vec3 position;
    glm::vec4 color;
    glm::vec2 uv;
    float textureIndex;
    int entityId;
};
```

Build gate:

- Colored quads render.
- Textured sprites render.
- Many sprites render in batches.
- Resize and orthographic projection work correctly.

## Phase 9: Game UI and HUD Layer

Goal: reuse and harden the engine-native UI stack for in-game HUD and menus.

Required widgets:

- Panels
- Buttons
- Text labels
- Icons
- Health/progress bars
- Inventory slots
- Crosshair
- Minimap placeholder
- Sliders
- Checkboxes
- Simple layout containers
- Anchors, margins, and padding
- Mouse hover and click
- Keyboard/controller navigation later

Important separation:

- UI layout, input, and state live in `UIRenderer` or a UI system.
- `Renderer2D` only draws primitives efficiently.

Suggested API:

```cpp
ui.begin();
ui.panel("Inventory");
ui.button("Use Item");
ui.text("Coins: 250");
ui.healthBar(currentHp, maxHp);
ui.end();
```

Build gate:

- HUD renders after 3D/post-process.
- Basic interactive button state works.
- Crosshair, health bar, and text are drawn with engine-native UI.

## Phase 10: Debug Renderer, Editor Overlays, and Gizmo Prep

Goal: add GPU-rendered debug and editor overlay primitives.

Required features:

- Lines
- Wire boxes and bounding boxes
- Spheres/circles if simple
- Transform axes
- Grid
- Debug labels
- Physics debug lines
- Raycast visualization
- Camera frustum visualization
- Light gizmos
- Object selection outlines

Gizmo preparation:

- Move arrows
- Rotate rings
- Scale handles
- Axis colors
- Object selection highlight
- Picking buffer later

Build gate:

- Debug lines and boxes render in world space.
- Debug labels render through text.
- Debug pass does not interfere with game HUD.

## Phase 11: Resource Management Foundation

Goal: add asset/resource ownership before serious model and texture loading.

Tasks:

- Add `ResourceManager`.
- Add typed handles or shared ownership rules for textures, meshes, models, materials, shaders, fonts, and audio clips.
- Add fallback resources: missing texture, default material, default font, default mesh.
- Add path-based caching.
- Add clear cleanup order for CPU and GPU resources.

Build gate:

- Loading the same logical resource twice returns the cached resource.
- Missing resources return fallbacks.
- Shutdown does not leak resource-owned objects.

## Phase 12: Modern Model Loading

Goal: load Blender-exported GLB/glTF models correctly.

Priority:

1. GLB/glTF
2. OBJ/MTL if already present
3. FBX later only if necessary

Required features:

- Multiple meshes
- Multiple submeshes
- Multiple materials
- Material assignment per mesh/submesh
- Positions, normals, tangents, UVs, vertex colors, indices
- Texture references
- Bounding boxes
- CPU metadata and GPU buffers
- Preserve skeleton and animation metadata for later

Do not flatten a model into one mesh and one material.

Build gate:

- A GLB with several meshes/materials loads.
- Each submesh keeps its material reference.
- Missing data falls back cleanly.

## Phase 13: Material System and PBR Basics

Goal: preserve glTF PBR materials and render them correctly enough for real assets.

Material properties:

- Base color / albedo
- Base color texture
- Normal map
- Roughness
- Metallic
- Roughness/metallic maps
- Ambient occlusion map
- Emissive color/map
- Opacity / alpha
- Alpha mode
- Double-sided flag
- UV scale/offset if practical
- Clearcoat/transmission placeholders

Material styles to prepare:

- PBR
- Unlit
- Toon later
- Debug
- Skybox
- Sprite
- Text

Build gate:

- glTF material assignments are preserved.
- Base color textures render.
- Normal maps affect lighting.
- Metallic/roughness values affect shading.
- Transparent materials are routed to a transparent pass.

## Phase 14: Lighting System

Goal: add physically reasonable lighting for PBR materials.

Required lights:

- Directional light
- Point light
- Spot light
- Ambient light
- Emissive material contribution

Prepare for:

- HDR rendering
- Tone mapping
- Bloom
- Image-based lighting
- Environment maps
- Reflection probes

Build gate:

- Imported models no longer look flat.
- Directional, point, and spot lights affect PBR materials.
- Point/spot attenuation behaves reasonably.

## Phase 15: Shadows

Goal: start with directional light shadows.

Required features:

- Shadow map render pass
- Depth texture
- Shadow sampler
- Bias handling
- Basic PCF filtering if practical
- Cast shadows / receive shadows flags
- Optional debug view for shadow map

Prepare for:

- Point light cube shadows
- Spot light shadows
- Cascaded shadow maps

Build gate:

- Directional light casts shadows.
- Meshes can opt in/out of casting and receiving shadows.
- Shadow artifacts are managed with basic bias.

## Phase 16: Post-Processing

Goal: render the 3D world to an offscreen framebuffer before HUD/UI.

Required features:

- Offscreen color/depth target
- Gamma correction
- Tone mapping
- Exposure
- HDR framebuffer if practical
- Bloom preparation or first implementation

Prepare for:

- Color grading
- FXAA/TAA
- Depth of field
- Motion blur

Build gate:

- 3D world renders through post-process.
- HUD/UI renders after post-process.
- Resize recreates offscreen resources safely.

## Phase 17: Scene, Entity, and Component Cleanup

Goal: establish a clean object model without overcomplicating ECS.

Core components:

- `TransformComponent`
- `MeshRendererComponent`
- `CameraComponent`
- `LightComponent`
- `AudioSourceComponent`
- `AudioListenerComponent`
- `RigidbodyComponent`
- `ColliderComponent`
- `ScriptComponent`

Transform:

- Position
- Rotation
- Scale
- Local matrix
- World matrix
- Parent/child support later

MeshRenderer:

- Model/mesh reference
- Material override references
- Visible flag
- Cast shadows
- Receive shadows

Build gate:

- Objects can be created, deleted, moved, rendered, and assigned components.
- Active camera and lights can come from scene components.

## Phase 18: Physics, Raycasting, and Picking Preparation

Goal: prepare physics and editor selection.

Physics options:

- Bullet
- Jolt
- PhysX
- Minimal custom physics only for learning/prototype purposes

Required features:

- Static and dynamic rigid bodies
- Box and sphere colliders
- Capsule collider later
- Mesh collider later
- Gravity
- Fixed physics update
- Raycast API
- Ray from camera through mouse
- Object picking using physics colliders or bounding boxes

Debug:

- Draw colliders.
- Draw rays.
- Draw hit points.

Build gate:

- Raycast API exists.
- Debug renderer can visualize rays and colliders.
- Editor picking path is prepared.

## Phase 19: Audio and 3D Audio

Goal: add audio objects tied to engine transforms.

Backend options:

- miniaudio
- OpenAL
- FMOD
- Wwise

Required features:

- Load sound
- Play/stop sound
- Looping
- Volume
- Pitch
- 2D sounds
- 3D positional sounds
- Listener position/orientation from camera/player
- Distance attenuation

Components:

- `AudioSourceComponent`
- `AudioListenerComponent`

Build gate:

- 2D sound plays.
- 3D sound position follows an entity transform.
- Listener follows camera/player transform.

## Phase 20: Scripting Preparation

Goal: start with native C++ scripts and keep the door open for Lua/C# later.

Required interfaces:

```cpp
class Script {
public:
    virtual ~Script() = default;
    virtual void onCreate() {}
    virtual void onUpdate(float deltaTime) {}
    virtual void onDestroy() {}
};
```

Scripts should eventually access:

- Transform
- Input
- Physics
- Audio
- Scene

Build gate:

- Script component can attach a native script.
- `onCreate`, `onUpdate`, and `onDestroy` are called at the right times.

## Phase 21: Editor Gizmos and Overlays

Goal: prepare editor rendering and selection tools.

Required features:

- Selection outline
- Move/rotate/scale gizmo placeholders
- Axis colors
- View grid
- Light and camera icons/gizmos
- Object picking integration
- Editor/game camera separation

Build gate:

- Selected objects can be highlighted.
- Gizmo/debug overlay pass renders after the scene.
- Picking API can identify objects or is cleanly stubbed.

## Phase 22: Animation and Bones Preparation

Goal: do not implement a fake animation system, but preserve the right data.

Prepare types:

- `Skeleton`
- `Bone`
- `Skin`
- `AnimationClip`
- `Keyframe`
- `AnimatorComponent`

Priority:

1. Load static GLB models correctly.
2. Preserve skeleton data if present.
3. Load animation clips later.
4. Add animator component later.
5. Add skinned mesh rendering later.

Build gate:

- Static model loading is not broken.
- Skeleton/animation metadata can be represented without being rendered yet.

## Phase 23: Resource Management Completion

Goal: centralize resource ownership and avoid duplicate loading.

ResourceManager should cover:

- Textures
- Meshes
- Models
- Materials
- Shaders
- Fonts
- Audio clips

Required behavior:

- Cache by path or asset id.
- Avoid duplicate GPU uploads.
- Provide fallbacks for missing texture, material, mesh, font, and shader.
- Clean up Vulkan resources safely.
- Prepare hot reload later.

Build gate:

- Loading the same texture/model twice reuses cached resources.
- Missing assets render with clear fallbacks.
- Shutdown cleans resources without validation errors.

## Phase 24: Scene Saving and Loading

Goal: prepare project and scene serialization.

Scene files should store:

- Objects
- Transforms
- Model references
- Material overrides
- Lights
- Cameras
- Audio sources
- Physics components
- Script references
- UI/HUD objects later

Do not store large binary model data directly in scene JSON unless there is a specific asset-pipeline reason.

Build gate:

- A simple scene can be serialized and restored.
- Asset references are stored as paths/ids.

## Phase 25: Multiple Render Styles

Goal: keep renderer and material design flexible.

Supported or prepared styles:

- Realistic PBR
- Unlit
- Debug
- Stylized
- Anime/cel-shaded later
- Toon outlines later

Toon roadmap:

- Toon ramp lighting
- Outline pass
- Flat colors
- Rim light
- Stylized shadows

Build gate:

- Materials can select or imply a shader/style path.
- Adding an unlit or toon shader later does not require rewriting the renderer.

## Phase 26: Shader Organization

Goal: keep shader code discoverable and aligned with engine structures.

Needed shaders:

- PBR mesh shader
- Unlit mesh shader
- Sprite/quad shader
- Text shader
- Debug line shader
- Skybox shader
- Shadow depth shader
- Post-process shader
- Outline/toon later

PBR shader inputs:

- Albedo/base color
- Normal map
- Metallic
- Roughness
- AO
- Emissive
- Alpha

Sprite shader inputs:

- Texture
- UV
- Tint
- Opacity
- Alpha blending

Text shader inputs:

- Font atlas
- Color
- Opacity
- MSDF support later

Build gate:

- Shader folders are organized by pass/style.
- Shader inputs match C++ structures.
- Recompilation/build scripts remain simple.

## Phase 27: Performance Rules and Audit

These rules apply throughout implementation, not only at the end.

Renderer:

- Do not recreate Vulkan pipelines every frame.
- Do not recreate descriptor sets unnecessarily.
- Do not upload unchanged textures every frame.
- Do not rebuild static mesh buffers every frame.
- Cache model GPU buffers.
- Cache texture GPU images.
- Use uniform buffers/storage buffers properly.
- Use push constants where appropriate.
- Minimize draw calls where reasonable.
- Sort by pipeline/material/texture when useful.
- Keep CPU/GPU synchronization safe.

2D:

- Batch quads, sprites, and text.
- Use atlas textures where possible.
- Use one index-buffer pattern for quads.
- Use dynamic vertex buffers or a ring buffer.

3D:

- Group by material/pipeline when practical.
- Reuse mesh buffers.
- Avoid reloading models.
- Avoid rebaking materials each frame.

## Final Acceptance Criteria

The upgraded engine should be able to:

- Load a GLB model exported from Blender.
- Preserve multiple meshes and materials.
- Render base color textures.
- Render normal maps.
- Render metallic/roughness materials.
- Render basic transparent materials.
- Render directional, point, and spot lights.
- Render at least directional-light shadows.
- Render a skybox.
- Render 2D sprites/quads after the 3D scene.
- Render text using a font atlas.
- Render HUD elements like health bar, crosshair, and ammo text.
- Render debug lines and bounding boxes.
- Prepare editor gizmos.
- Prepare object picking and raycasting.
- Prepare or implement 3D audio.
- Prepare script components.
- Compile as valid C++ at every phase.
- Keep Vulkan resources managed safely.
- Keep architecture modular and extendable.

## Recommended Implementation Order

1. Finish project hygiene: README, build docs, dependency docs, and clean git state.
2. Build the engine app shell: `Application`, `Window`, `Time`, `Input`, and `Log`.
3. Add minimal Vulkan frame: instance, surface, device, swapchain, command buffers, frame sync, clear color.
4. Add minimal Renderer2D foundation for colored UI rectangles.
5. Add engine-native editor UI shell: panels, buttons, toggles, sliders, inputs, toolbar, hierarchy, inspector, console, viewport placeholder.
6. Add TextRenderer with font atlas so editor UI has real labels and input text.
7. Add editor viewport integration inside the native UI layout.
8. Add clean renderer architecture and render pass order.
9. Expand Renderer2D into a full batched sprite/quad renderer.
10. Reuse/harden the UI stack for in-game HUD and menus.
11. Add DebugRenderer for lines, boxes, and labels.
12. Add ResourceManager foundation before complex asset loading.
13. Upgrade model loading for GLB/glTF multiple meshes/materials.
14. Add material system and PBR shader basics.
15. Add lighting system.
16. Add shadows.
17. Add post-processing.
18. Add scene/entity/component cleanup.
19. Add physics/raycast/picking preparation.
20. Add audio/3D audio.
21. Add scripting preparation.
22. Add editor gizmos and overlays.
23. Add animation/bones preparation.
24. Add scene serialization.
25. Add multiple render style support.
26. Complete shader organization and performance audit.

## Current Phase Tracker

Update this section as work progresses.

```text
[x] Phase 0  - Project bootstrap
[x] Phase 1  - Engine app shell
[ ] Phase 2  - Minimal Vulkan frame
[ ] Phase 3  - Minimal Renderer2D foundation
[ ] Phase 4  - Engine-native editor UI shell
[ ] Phase 5  - Text rendering
[ ] Phase 6  - Editor viewport integration
[ ] Phase 7  - Clean render architecture
[ ] Phase 8  - Full Renderer2D batching/sprites
[ ] Phase 9  - Game UI/HUD system
[ ] Phase 10 - Debug renderer
[ ] Phase 11 - Resource management foundation
[ ] Phase 12 - GLB/glTF model loading
[ ] Phase 13 - Material/PBR system
[ ] Phase 14 - Lighting
[ ] Phase 15 - Shadows
[ ] Phase 16 - Post-processing
[ ] Phase 17 - Scene/components
[ ] Phase 18 - Physics/raycast/picking
[ ] Phase 19 - Audio/3D audio
[ ] Phase 20 - Scripting
[ ] Phase 21 - Editor gizmos/overlays
[ ] Phase 22 - Animation preparation
[ ] Phase 23 - Resource management completion
[ ] Phase 24 - Scene serialization
[ ] Phase 25 - Multiple render styles
[ ] Phase 26 - Shader organization
[ ] Phase 27 - Performance audit
```

## Notes for Future Edits

- If a later implementation discovers a better order, update this plan instead of forcing the engine into the original sequence.
- If an existing module already covers a responsibility, evolve it rather than duplicating it.
- Each completed phase should add a short note describing what changed, what was deferred, and how it was tested.
