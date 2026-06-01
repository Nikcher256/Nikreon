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
- Organize shaders by purpose and rendering feature so PBR, unlit, stylized, debug, sprite, text, shadow, skybox, outline, voxel/block, and post-process passes can coexist without forcing the whole engine into one predefined art style.

## Renderer Art Direction Goal

The engine should not have a hardcoded "style selector" that limits games to a few named looks. The renderer should provide flexible building blocks so a project can create whatever visual direction it needs:

- Modern realistic or AAA-like scenes with PBR materials, normal maps, lighting, shadows, reflections, transparency, HDR, and post-processing.
- Stylized or anime-like scenes using custom shader paths, ramp textures, outlines, rim lighting, flat colors, emissive accents, or hand-authored material parameters.
- Low-poly games with simple materials, flat shading options, vertex colors, low-cost lighting, and minimal post-processing.
- Block/voxel/Minecraft-like tests using atlased textures, chunk meshes, crisp sampling, simple lighting, and optional block-outline/debug tools.
- Editor/debug rendering with wireframes, IDs, selection outlines, gizmos, collision shapes, raycasts, and labels.

This means the 3D renderer should be data-driven and extensible:

- Models keep their real mesh/submesh/material structure.
- Materials describe properties and shader features, not just one fixed PBR layout.
- Render passes are modular: opaque, transparent, shadows, skybox, post-process, outlines, debug, and editor overlays can be enabled as needed.
- Shaders are registered by purpose/features, so new looks can be added without rewriting `Renderer3D`.
- glTF/GLB import should preserve standard data, and engine-specific extras can optionally add custom material/render hints later.
- Blender-specific viewport/render tricks should not be assumed to transfer automatically; the engine should recreate those looks through explicit renderer features such as outlines, custom shaders, ramps, post-process, and material parameters.

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
- Window close exits the app.
- Logs show startup/shutdown.
- No Vulkan device/swapchain required yet.

Completed note:

- Added `Application`, `Window`, `Time`, `Input`, and `Log`.
- `main.cpp` now creates and runs `Engine::Application`.
- Added `--smoke-test` mode for one-frame startup/shutdown verification.
- Verified with `scripts/build.ps1` and `build/Debug/NikreonEngine.exe --smoke-test`.

## Phase 2: Minimal Vulkan Frame

Status: complete.

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

Completed note:

- Added `VulkanContext` with volk initialization, Vulkan instance, debug messenger, surface, physical/logical device, swapchain, image views, command pool, command buffers, semaphores, and fences.
- The frame currently clears the swapchain image directly with `vkCmdClearColorImage`.
- Verified with `scripts/build.ps1` and `build/Debug/NikreonEngine.exe --smoke-test`.
- Verified semaphore reuse with `build/Debug/NikreonEngine.exe --frames 180`.

## Phase 3: Minimal Renderer2D Foundation

Status: complete.

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

Completed note:

- Added shader compilation through CMake using `glslc`.
- Added an API-neutral `Renderer2D` interface and a Vulkan-specific `VulkanRenderer2D` implementation.
- Added `Renderer` as the high-level renderer orchestrator above the active backend.
- Added a simple colored-quad Vulkan graphics pipeline.
- Added swapchain render pass and framebuffers.
- The current frame batches editor shell quads into draw calls through the renderer abstraction.
- Verified with `scripts/build.ps1` and `build/Debug/NikreonEngine.exe --frames 180`.

## Phase 4: Engine-Native UI Foundation and Editor Shell

Status: in progress.

Goal: create reusable native UI controls first, using the engine's own 2D/UI renderer.

Important:

- This is the first real UI users can play with.
- Keep UI logic separate from `Renderer2D`.
- `Renderer2D` draws primitives only; widgets should not know Vulkan.
- `Engine/UI` owns reusable UI primitives that will later move with text rendering into a separate `NikreonUI` GitHub project.
- `EditorUI` handles editor layout/composition and editor-specific panels.
- Prefer a small retained widget model for editor UI instead of piling everything into immediate-mode helper calls.

Tasks:

- Add `UIContext`.
- Add retained `Widget` base class.
- Add `Button`, `Checkbox`, and `Slider`.
- Add callbacks such as `onClick` and `onValueChanged`.
- Add `UIStyle` and box/widget style structs.
- Add per-widget style overrides.
- Add reusable style classes.
- Add SDF styled-box rendering so radius/border style values are real.
- Add `Editor/EditorLayer`.
- Add `Editor/EditorUI`.
- Add panels/windows drawn as styled boxes.
- Add buttons.
- Add checkboxes/toggles.
- Add sliders.
- Add text input boxes, even if text rendering starts with placeholder rectangles/caret.
- Add simple layout: rows, columns, padding, margin.
- Add hover, active, focused, clicked states.
- Add basic theme colors and widget-state colors.
- Add top toolbar with Play, Pause, Stop buttons.
- Add left hierarchy placeholder panel.
- Add right inspector placeholder panel.
- Add bottom console/log placeholder panel.
- Add center viewport placeholder panel.
- Add a CSS-like style file parser later for the subset of style properties the engine supports.
- Add scrollable panel content when controls do not fit vertically.
- Add responsive panel visibility rules so low-height or narrow windows can collapse/hide secondary panels while preserving a usable viewport.

Build gate:

- User can click buttons.
- User can toggle checkboxes.
- User can drag sliders.
- User can type into an input box once text support exists, or see a clear placeholder until text is implemented.
- Panels resize with the window.
- Panels with overflowing content can scroll after clipping support exists.
- Panels can collapse or hide responsively when the window becomes too small.
- Center viewport placeholder exists but does not need to render 3D yet.
- Styled boxes visibly respect border width and border radius.

Completed/started note:

- Added `EditorLayer` and `EditorUI`.
- The editor shell now draws native toolbar, hierarchy, inspector, console, and viewport-placeholder panels through `Renderer2D`.
- Added `UIContext` with mouse position, hover, active, click, checkbox, and slider interaction state.
- Toolbar buttons, hierarchy row selection, inspector checkbox/slider placeholders, and viewport focus are now interactive.
- Added a core `Layer` and `LayerStack` so application update/render flow can support editor layers, game layers, UI overlays, debug overlays, and future tool layers.
- Current visual feedback is rectangle/icon based until the text renderer exists: toolbar state, selected hierarchy rows, viewport focus, grid toggle, and brightness slider should visibly change.
- Refactored UI toward retained widgets: `Widget`, `Button`, `Checkbox`, and `Slider` now own interaction state and callbacks such as `onClick` and `onValueChanged`.
- Added `UIStyle` and box/widget style structs for colors, borders, spacing, padding, and future border-radius rendering.
- Added per-widget style overrides and reusable style classes so individual widgets can opt into custom visuals without changing every widget of that type.
- Added a Vulkan SDF rectangle path for styled UI boxes so `borderRadius`, `borderWidth`, fill, and border colors now render through the GPU instead of being only style data.
- Refactored SDF rectangle rendering to use a static quad vertex buffer plus one instance record per box, with flat shader inputs for per-rectangle style data.
- `EditorUI` now composes widgets and lays them out instead of hardcoding all widget logic inline.
- Current SDF UI boxes are batched with instancing: one shared quad, one instance per box.
- Added reusable UI layout containers under `Engine/UI`: row, column, stack, dock/fill, padding, gap, cross-axis alignment, and anchors.
- Refactored `EditorUI` to use the layout containers for the panel shell, toolbar buttons, hierarchy rows, and inspector controls.
- Added a deliberately small CSS-like style parser for supported UI properties and style classes.
- Added `assets/styles/editor.ui.css` and moved the editor shell's style-class overrides out of C++.
- Added stylesheet text classes with color, font, scale, opacity, horizontal alignment, vertical alignment, and offset properties.
- Replaced editor label magic offsets with measured bounds-based text placement and reserved panel-header spacing.
- Added CSS-like selector support for `type`, `type.class`, `type#id`, and `type.class#id`.
- Widget and text ID selectors now override reusable classes; class-plus-ID selectors inherit the reusable class before applying ID-specific properties.
- Added user-draggable hierarchy, inspector, and console splitters with clamped panel extents.
- Added responsive editor-shell visibility rules that preserve the viewport by hiding secondary panels at narrow or low window sizes.
- Added a retained scrub-style `NumberInput` widget with value clamping, precision formatting, sensitivity, callbacks, and CSS-like `number-input` styles.
- Sliders expose formatted values so editor controls can render Unreal-like numeric overlays.
- Added GLFW character, editing-key, and wheel event queues exposed through engine input snapshots.
- Added retained `TextInput` with focus, UTF-8 insertion, UTF-8-boundary caret movement, shift-selection ranges, deletion, placeholder text, callbacks, and CSS-like `text-input` styles.
- Added nested renderer clip rectangles and a reusable wheel-driven `ScrollContainer`; inspector shapes and labels now scroll inside a clipped content region.
- Added explicit toolbar controls for collapsing hierarchy, inspector, and console panels.
- Bundled redistributable Noto Sans plus its license under `assets/fonts` and prefer it before optional system-font fallbacks.
- Expanded the default atlas to Latin-1 and Cyrillic ranges with dynamic atlas-height growth.
- Added `TextLayout` options for configurable line spacing, maximum width, and basic wrapping.
- Added clipped hit testing and draggable scrollbar thumbs to `ScrollContainer`.
- Raw mouse coordinate debug logs were removed; useful widget action logs remain.
- Text labels and typed text editing are active; proper icons are still pending.

Next UI foundation tasks:

- Add copy/paste shortcuts and richer mouse-driven text selection gestures.
- Do not implement full browser CSS: no cascade complexity, media queries, full selector engine, or DOM model.
- Keep `Engine/UI` and text APIs independent from Vulkan so they can be extracted into a reusable `NikreonUI` GitHub project.

## Phase 5: Text Rendering

Status: in progress.

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

Completed/started note:

- Added an API-neutral `TextRenderer` interface and Vulkan-specific `VulkanTextRenderer` backend.
- Added FreeType TTF loading for Latin-1 and Cyrillic glyphs, cached glyph metrics, growing atlas height, UTF-8 decoding, text measurement, scaling, color, opacity, alignment, configurable line spacing, basic wrapping, and multi-line positioning.
- Added one-time staging-buffer upload for a cached single-channel Vulkan font atlas.
- Added a Vulkan text pipeline with descriptor-backed atlas sampling and batched dynamic glyph vertices.
- Added `text.vert` and `text.frag` shader compilation through CMake.
- Renderer orchestration now records text after 2D UI shapes so editor labels remain crisp and visible.
- Added real editor labels for panels, hierarchy rows, inspector controls, and the console placeholder.
- Editor labels now resolve typography from CSS-like text classes and align measured text within explicit UI rectangles.
- Bundled Noto Sans is the redistributable default font; common system-font locations remain optional fallbacks.
- Verified with `scripts/build.ps1`, `build/Debug/NikreonEngine.exe --smoke-test`, and `build/Debug/NikreonEngine.exe --frames 180`.

Remaining text tasks:

- Add on-demand Unicode glyph atlas growth or multiple atlas pages beyond the preloaded Latin-1 and Cyrillic ranges.
- Improve wrapping from the basic glyph-width pass to word-boundary-aware rich text layout.
- Add clipboard shortcuts and richer mouse-driven selection gestures.

## NikreonUI Extraction Milestone

Status: in progress.

Goal: move reusable UI and text code into its own GitHub project after the first `TextRenderer` pass works, so the library can be versioned independently and reused by other games and tools.

Important:

- Create a separate `NikreonUI` GitHub repository with its own CMake library target.
- Move reusable widgets, layouts, style parsing, text layout, font-atlas ownership, and shared UI assets/shaders into `NikreonUI`.
- Keep editor-specific composition such as hierarchy, inspector, console, and viewport panels inside `NikreonEngine`.
- Keep Vulkan-specific engine ownership behind a narrow rendering adapter. `NikreonUI` should submit styled boxes, clipped regions, sprites, and glyph quads without owning the engine swapchain or frame lifecycle.
- Pin the dependency revision from `NikreonEngine` so UI updates are intentional and reproducible.
- Do this extraction after basic text rendering works and before finishing the remaining UI polish tasks, avoiding churn while the first font-atlas path is still taking shape.

Build gate:

- `NikreonUI` builds as a standalone library repository.
- `NikreonEngine` consumes a pinned `NikreonUI` revision through CMake.
- Another small sample target can render a styled panel and text without depending on editor code.
- UI and text changes can be released independently from the engine.

Completed/started note:

- Extracted reusable widgets, layouts, style parsing, renderer interfaces, Vulkan 2D/text backends, and shared shaders into `external/NikreonUI`.
- Added a standalone `NikreonUI` CMake project, `NikreonUI::NikreonUI` library target, vcpkg manifest, README, and ignore file.
- Removed the UI library's dependency on engine input and GLFW; consumers now pass a small `UIInputState` snapshot.
- Removed the UI library's dependency on the engine logger.
- Updated `NikreonEngine` to link `NikreonUI::NikreonUI` through configurable `NIKREON_UI_SOURCE_DIR`.
- Initialized the standalone local git repository and committed baseline revision `3ee0eaa61efcc07795365be8cbfccd0212f644c9`.
- Published the standalone public repository at `https://github.com/Nikcher256/NikreonUI`.
- Registered `external/NikreonUI` as a Git submodule pinned to baseline revision `3ee0eaa61efcc07795365be8cbfccd0212f644c9`.
- Added build preflight scripts that initialize a missing `external/NikreonUI` submodule checkout before CMake runs.
- Verified both the integrated engine build and standalone `NikreonUI` build.

Remaining extraction tasks:

- Add a small standalone sample executable that renders one styled panel and text.

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

Current note:

- Renderer2D already has colored quads and an SDF styled-rectangle path.
- SDF rectangles are drawn efficiently with instancing: one static quad vertex buffer and one per-rectangle instance buffer.
- The remaining large missing piece for UI/HUD is textured quads/sprites, which unlocks icons, image buttons, font atlases, thumbnails, and game sprites.

Required features:

- Batched colored quads
- Batched textured quads
- Batched SDF styled rectangles
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
- For repeated rectangle-like primitives, prefer instancing where the shape is shared and per-object data varies.
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
- SDF rounded rectangles render with border radius and border width.
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

Material/shader flexibility to prepare:

- PBR materials for standard glTF/GLB assets.
- Unlit materials for UI-like 3D, icons, flat props, emissive props, and stylized assets.
- Custom material parameters so projects can add ramps, thresholds, rim light, outline settings, vertex-color use, texture atlases, or other look-specific controls.
- Material feature flags instead of one hardcoded material type.
- Shader variants or shader programs selected from material features and render pass needs.
- Debug/editor materials for selection, IDs, wireframes, gizmos, and physics visualization.
- Skybox, sprite, text, shadow, post-process, and outline shaders as separate renderer tools rather than global art-style choices.

Build gate:

- glTF material assignments are preserved.
- Base color textures render.
- Normal maps affect lighting.
- Metallic/roughness values affect shading.
- Transparent materials are routed to a transparent pass.
- Materials can carry extra renderer hints without breaking standard glTF loading.

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

## Phase 25: Flexible Render Features

Goal: keep renderer and material design open-ended so projects can create many visual directions without the engine forcing a fixed style list.

Renderer features to support or prepare:

- Standard PBR shading for realistic assets.
- Unlit and flat-shaded materials.
- Vertex color support.
- Texture atlas workflows for sprites, low-poly props, voxel/block games, and stylized assets.
- Optional outline rendering for characters, selected objects, interactables, debug overlays, or stylized effects.
- Optional ramp/threshold shading support through material parameters and shader variants.
- Optional rim light, emissive accents, and custom material constants.
- Debug/editor rendering paths for IDs, wireframes, gizmos, physics shapes, and object picking.
- Post-process hooks for tone mapping, bloom, color grading, pixelation, posterization, or other project-specific effects.

Important:

- Do not build a global "style selector" into the engine.
- Do not make `Renderer3D` assume one art direction.
- Treat looks as combinations of material data, shader features, mesh data, render passes, lighting, and post-processing.
- New visual directions should be added by registering materials/shaders/passes, not by rewriting the renderer core.

Build gate:

- Materials can request shader features without hardcoding a project style.
- Adding an outline, unlit, ramp, voxel/block, or custom shader path later does not require rewriting `Renderer3D`.

## Phase 26: AI Editor Assistant Support

Goal: add an optional editor tool that can turn prompts into structured, validated engine data and editor commands.

This is an editor/productivity feature, not part of the Vulkan renderer core.

Possible AI-assisted actions:

- Create scene objects from prompts.
- Add or configure components.
- Suggest material overrides.
- Create light setups.
- Generate UI layout descriptions.
- Fill script-template parameters.
- Suggest debug/editor setup changes.
- Produce scene diffs for review.

Important rules:

- AI must not directly control Vulkan resources, pipelines, buffers, descriptor sets, render passes, or renderer internals.
- AI output must be structured data or explicit editor commands, not arbitrary engine mutation.
- AI output must be validated against scene schemas, component schemas, asset registries, and project settings before applying.
- The editor must show a preview/diff before applying AI changes.
- AI should use available asset lists and scene schemas so it does not invent invalid asset paths or component names.
- Raw generated code should not be compiled automatically without user review and sandboxing.
- AI actions should be undoable through the normal editor command/undo system.
- AI failures should leave the scene unchanged.

Suggested architecture:

- `AIEditorAssistant`
- `AICommandSchema`
- `AICommandValidator`
- `AICommandPreview`
- `AIApplyCommand`
- integration with `Scene`, `ResourceManager`, asset browser, component registry, material system, and scripting templates

Example command types:

```json
{
  "type": "create_entity",
  "name": "Key Light",
  "components": [
    {
      "type": "TransformComponent",
      "position": [0.0, 4.0, 2.0]
    },
    {
      "type": "LightComponent",
      "lightType": "directional",
      "intensity": 3.0
    }
  ]
}
```

Build gate:

- AI can produce a validated command list without directly mutating renderer internals.
- Editor shows preview/diff before apply.
- Invalid asset paths or component names are rejected.
- Applying accepted commands updates the scene through normal editor command paths.
- Raw generated code is never compiled automatically.

## Phase 27: Shader Organization

Goal: keep shader code discoverable and aligned with engine structures.

Needed shaders:

- PBR mesh shader
- Unlit/flat mesh shader
- Custom material shader variants
- Sprite/quad shader
- Text shader
- Debug line shader
- Skybox shader
- Shadow depth shader
- Post-process shader
- Optional outline shader/pass
- Optional ramp/threshold shading variant

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

## Phase 28: Performance Rules and Audit

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
5. Add engine-native UI foundation: layer stack, retained widgets, callbacks, style structs, style classes, per-widget overrides, SDF styled boxes, toolbar, hierarchy, inspector, console, viewport placeholder.
6. Add `TextRenderer` with font atlas so editor UI has real labels and input text.
7. Extract reusable UI and text code into the standalone `NikreonUI` GitHub project with a pinned CMake dependency.
8. Finish UI foundation: labels, text input, scrollable clipped panels, and responsive panel collapse/hide rules.
9. Add textured Renderer2D/sprite support for icons, image buttons, thumbnails, font atlases, and game sprites.
10. Add editor viewport integration inside the native UI layout.
11. Add clean renderer architecture and render pass order.
12. Reuse/harden the UI stack for in-game HUD and menus.
13. Add DebugRenderer for lines, boxes, and labels.
14. Add ResourceManager foundation before complex asset loading.
15. Upgrade model loading for GLB/glTF multiple meshes/materials.
16. Add material system and PBR shader basics.
17. Add lighting system.
18. Add shadows.
19. Add post-processing.
20. Add scene/entity/component cleanup.
21. Add physics/raycast/picking preparation.
22. Add audio/3D audio.
23. Add scripting preparation.
24. Add editor gizmos and overlays.
25. Add animation/bones preparation.
26. Add scene serialization.
27. Add flexible render feature support.
28. Add optional AI editor assistant support using validated structured commands.
29. Complete shader organization and performance audit.

## Current Phase Tracker

Update this section as work progresses.

```text
[x] Phase 0  - Project bootstrap
[x] Phase 1  - Engine app shell
[x] Phase 2  - Minimal Vulkan frame
[x] Phase 3  - Minimal Renderer2D foundation
[ ] Phase 4  - Engine-native UI foundation/editor shell
[~] Phase 5  - Text rendering
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
[ ] Phase 25 - Flexible render features
[ ] Phase 26 - AI editor assistant
[ ] Phase 27 - Shader organization
[ ] Phase 28 - Performance audit
```

## Notes for Future Edits

- If a later implementation discovers a better order, update this plan instead of forcing the engine into the original sequence.
- If an existing module already covers a responsibility, evolve it rather than duplicating it.
- Each completed phase should add a short note describing what changed, what was deferred, and how it was tested.
