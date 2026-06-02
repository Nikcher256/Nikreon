# Modern Vulkan Game Engine Upgrade Plan

This document is the working roadmap for upgrading the C++ Vulkan engine into a modular modern game engine. It is intentionally phased: each phase should compile, preserve existing working behavior where possible, and avoid rewriting unrelated systems.

The main target is a renderer and engine architecture that can load Blender-exported GLB/glTF assets with proper materials, textures, lighting, transparency, shadows, and later animation, while also supporting a shared GPU-rendered UI system and a separate engine 2D world renderer.

## Current Starting Point

The repository now has the project bootstrap in place:

- Git repository initialized.
- CMake project created.
- `Makefile` and platform scripts added for Windows, Linux, and macOS.
- `vcpkg.json` dependency manifest added.
- Core external libraries selected: GLFW, GLM, Vulkan headers, volk, stb, tinygltf, FreeType, OpenAL Soft, miniaudio, EnTT, spdlog, nlohmann-json, and Jolt Physics.
- Minimal `src/main.cpp` builds and runs.

The next work should not jump directly into complex PBR rendering. The safer path is to build a small engine skeleton first, add minimal Vulkan ownership, build the engine-native UI layer for editor controls, then add viewport modes before complex 2D and 3D world rendering.

## Guiding Rules

- Keep existing working code unless a change is needed for the current phase.
- Do not build one huge renderer file. Split responsibility into small engine modules.
- Every phase should build before moving on.
- Prefer glTF/GLB as the primary Blender import path.
- UI and game-world 2D rendering are separate responsibilities. Do not combine them into one catch-all renderer.
- `NikreonUI` is the shared UI/HUD/menu system. Use it for editor UI, in-game HUD, game menus, pause menus, inventory UI, dialogue boxes, buttons, sliders, text inputs, panels, layouts, text, styles, progress bars, icons, crosshairs, and nine-slice panels.
- `NikreonUI` must not become the engine's sprite, tilemap, or particle renderer.
- The engine 2D world renderer handles game-world sprites, tilemaps, particles, parallax layers, sprite animations, 2D camera/world transforms, and 2D debug drawing.
- `EditorUI` and `GameHUD` are separate composition layers that can both use `NikreonUI`.
- Do not use ImGui. Editor UI, game HUD, menus, debug panels, and tools should be engine-native and GPU-rendered through `NikreonUI`.
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

Exported game:

1. Shadow pass
2. 3D and/or 2D game-world pass
3. Skybox/background
4. Transparent world pass
5. Post-processing
6. `GameHUD` pass using `NikreonUI`
7. Present

Editor:

1. Render the scene into the viewport render target using the editor camera or game camera depending on viewport mode.
2. Render `GameHUD` into the viewport target only in `Play` or `HudEdit`. Exported runtime uses its own game render path.
3. Draw the viewport texture inside the editor viewport panel.
4. Render editor debug overlays/gizmos if needed.
5. Render `EditorUI` using `NikreonUI`.
6. Present.

`GameHUD` normally renders after post-processing so text, crosshairs, menus, and health bars are not blurred by bloom, depth of field, or tone mapping artifacts.

## UI Layers, Surfaces, and Viewport Modes

`NikreonUI` is a reusable UI system, not a game-world renderer. Both editor and game composition layers use it:

- `EditorUI`: toolbar, hierarchy, inspector, console, asset browser, and editor viewport frame.
- `GameHUD`: health bar, ammo, crosshair, inventory, pause menu, dialogue UI, minimap, and game settings.

`GameHUD` visibility is mode-specific:

- Show it in `Play` mode.
- Show it in the exported/runtime game.
- Show it in the dedicated `HudEdit` mode.
- Hide it by default in normal `Edit` mode.
- Keep it normally hidden in `Simulate` mode unless the user explicitly previews it.

Viewport modes:

- `Edit`: use the editor/debug fly camera, show editor gizmos, and hide `GameHUD` by default.
- `Play`: use the scene primary game camera, route input to the game and `GameHUD`, and render `GameHUD`.
- `Simulate`: run simulation while keeping the editor camera; normally hide `GameHUD` unless previewed.
- `HudEdit`: show `GameHUD` for editing without full `Play` mode, with anchors, safe areas, and layout guides.

`GameHUD` renders in game viewport-local coordinates through a UI surface:

```cpp
struct UISurface {
    glm::vec2 origin;
    glm::vec2 size;
    float scale;
};
```

For `GameHUD`, `{0, 0}` is the top-left of the game viewport or render target. In editor `Play` and `HudEdit` modes, `GameHUD` renders into the editor viewport render target. In exported game mode, it renders into the game window/swapchain. The same `GameHUD` code should work in both cases.

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
    Renderer2DWorld
    NikreonUIAdapter
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
  Game/
    GameHUD
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

- This completed phase is the historical foundation for GPU-rendered UI primitives.
- It is not the engine 2D world renderer for sprites, tilemaps, particles, or 2D cameras.
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

Goal: create reusable native UI controls first, using the engine's own GPU-rendered UI path.

Important:

- This is the first real UI users can play with.
- Keep UI logic separate from low-level primitive rendering.
- Low-level primitive rendering draws efficiently; widgets should not know Vulkan.
- During the initial implementation, `Engine/UI` owns reusable UI primitives that move with text rendering into the separate `NikreonUI` GitHub project.
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
- Added a retained scrub-style `NumberInput` widget with value clamping, precision formatting, sensitivity, callbacks, CSS-like `number-input` styles, optional plain coordinate-style rendering without a value bar, and click-to-edit keyboard entry composed from `TextInput`.
- Added an inspector coordinate-row example composed from plain X/Y/Z `NumberInput` blocks with a live `{x, y, z}` summary.
- Sliders expose formatted values and click-to-edit keyboard entry composed from `TextInput`, so editor controls can render Unreal-like numeric overlays while retaining drag interaction.
- Added GLFW character, editing-key, and wheel event queues exposed through engine input snapshots.
- Added retained `TextInput` with focus, UTF-8 insertion, UTF-8-boundary caret movement, click-to-position, mouse-drag and shift-selection ranges, visible post-text selection overlays, Ctrl+A/C/V clipboard shortcuts, automatic and thumb-draggable horizontal overflow scrolling, deletion, placeholder text, callbacks, and CSS-like `text-input` styles.
- Added nested renderer clip rectangles and a reusable wheel-driven `ScrollContainer`; inspector shapes and labels now scroll inside a clipped content region.
- Added explicit toolbar controls for collapsing hierarchy, inspector, and console panels.
- Bundled redistributable Noto Sans plus its license under `assets/fonts` and prefer it before optional system-font fallbacks.
- Expanded the default atlas to Latin-1 and Cyrillic ranges with dynamic atlas-height growth.
- Added `TextLayout` options for configurable line spacing, maximum width, and basic wrapping.
- Added clipped hit testing and draggable scrollbar thumbs to `ScrollContainer`.
- Raw mouse coordinate debug logs were removed; useful widget action logs remain.
- Text labels and typed text editing are active; proper icons are still pending.

Next UI foundation tasks:

- Add richer selection gestures such as double-click word selection.
- Do not implement full browser CSS: no cascade complexity, media queries, full selector engine, or DOM model.
- Keep the extracted `NikreonUI` widget and text APIs independent from Vulkan-facing engine ownership.

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
- Add richer selection gestures such as double-click word selection.

## NikreonUI Extraction Milestone

Status: in progress.

Goal: move reusable UI and text code into its own GitHub project after the first `TextRenderer` pass works, so the library can be versioned independently and reused by other games and tools.

Important:

- Create a separate `NikreonUI` GitHub repository with its own CMake library target.
- Move reusable widgets, layouts, style parsing, text layout, font-atlas ownership, and shared UI assets/shaders into `NikreonUI`.
- Use `NikreonUI` for `EditorUI`, `GameHUD`, game menus, pause menus, inventory UI, dialogue boxes, buttons, sliders, text inputs, panels, layouts, text, styles, progress bars, icons, crosshairs, and nine-slice panels.
- Keep `NikreonUI` focused on UI surfaces. It must not become the engine's sprite, tilemap, particle, parallax, sprite-animation, or 2D camera renderer.
- Keep editor-specific composition such as hierarchy, inspector, console, and viewport panels inside `NikreonEngine`.
- Keep game-specific HUD composition such as health, ammo, crosshair, inventory, minimap, pause menu, dialogue UI, and game settings inside the game layer.
- Keep Vulkan-specific engine ownership behind a narrow rendering adapter. `NikreonUI` should submit styled boxes, clipped regions, UI images/icons, and glyph quads without owning the engine swapchain or frame lifecycle.
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

Goal: make the editor viewport panel the place where the engine renders the game/editor scene and establish viewport modes before complex renderer work.

Tasks:

- Add a viewport render target abstraction.
- Render the Vulkan clear color or test scene into the viewport panel.
- Track viewport size and mouse position.
- Add viewport focus/hover state.
- Add editor camera placeholder controls.
- Prepare picking coordinates from viewport-local mouse position.
- Keep menu/panels independent from viewport rendering.
- Add explicit `Edit`, `Play`, `Simulate`, and `HudEdit` viewport modes.
- In `Edit`, use the editor/debug fly camera, show editor gizmos, and hide `GameHUD` by default.
- In `Play`, use the scene primary game camera, route input to the game and `GameHUD`, and render `GameHUD`.
- In `Simulate`, run simulation with the editor camera and normally hide `GameHUD` unless previewed.
- In `HudEdit`, render `GameHUD` without full `Play` mode and show anchors, safe areas, and layout guides.

Build gate:

- Main editor window has UI panels plus a working viewport panel.
- Viewport resizes without breaking the swapchain/render target.
- Editor UI remains interactive while the viewport updates.
- Viewport mode state exists before complex 2D or 3D renderer work depends on it.
- Normal `Edit` mode does not show `GameHUD`.

Completed note:

- Added an engine-owned Vulkan viewport render target that resizes to the editor viewport panel and clears to a mode-specific test color before copying into the panel.
- Added an inspector color-swatch button with a nearby compact Unreal-inspired popup: saturation/value palette, hue strip, old/new previews, RGB fine-tuning tracks, and editable `0-255` RGB number fields. The editor background leaves the viewport target visible so the selected clear color can be checked interactively beneath editor overlays.
- Added viewport-local mouse, hover, focus, and picking-coordinate tracking without coupling editor panels to Vulkan ownership.
- Added explicit `Edit`, `Play`, `Simulate`, and `HudEdit` modes plus focused-only placeholder editor fly-camera movement for `Edit` and `Simulate`.
- Added focused non-GPU tests for viewport interaction, picking coordinates, modes, and camera-input gating.
- Deferred game-world 2D, 3D, materials, real cameras, `GameHUD`, and scene picking to their later phases.

## Phase 7: Clean Render Architecture

Goal: introduce the architecture and render pass order without breaking existing rendering.

Tasks:

- Add or prepare the main renderer orchestration layer.
- Add empty or minimal modules for `Renderer3D`, `Renderer2DWorld`, the `NikreonUI` rendering adapter, `DebugRenderer`, `ShadowRenderer`, `SkyboxRenderer`, and `PostProcessRenderer`.
- Define a frame render sequence matching the target render order.
- Add clear interfaces for per-frame begin/end, resize, resource cleanup, and command buffer recording.
- Prepare separate placeholders for `EditorUI`, viewport-local `GameHUD`, engine 2D world rendering, world-space UI/billboards, editor overlays, and debug labels.

Build gate:

- Existing editor UI and viewport placeholder still render.
- New modules compile even if most are placeholders.
- No Vulkan resources are leaked or recreated per frame unnecessarily.
- The architecture does not route game-world sprites, tilemaps, or particles through `NikreonUI`.

## Phase 8: Engine 2D World Renderer, Sprites, and Tilemaps

Goal: add a dedicated batched GPU renderer for 2D game-world content. This is not the HUD/menu renderer.

Current note:

- The completed early `Renderer2D` work established reusable GPU primitive techniques for UI.
- `NikreonUI` owns UI concerns such as styled rectangles, clipping, icons, text, panels, and HUD widgets.
- The engine still needs a separate 2D world renderer for scene content.

Required features:

- Batched 2D game-world sprites
- Sprite sheets and texture atlases
- Tilemaps
- Particles
- Parallax layers
- Sprite animations
- Rotation, scaling, tint color, opacity, and UV coordinates
- Z/layer sorting
- 2D camera and world transforms
- Orthographic world projection
- 2D debug drawing
- World-space lines, rectangle outlines, filled rectangles, and simple circles if practical

Suggested API:

```cpp
renderer2DWorld.begin(camera);
renderer2DWorld.drawSprite(texture, transform, uv, tint);
renderer2DWorld.drawTilemap(tilemap, transform);
renderer2DWorld.drawParticles(particleSystem);
renderer2DWorld.drawDebugLine(start, end, color, thickness);
renderer2DWorld.end();
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
- Do not add HUD widget state, menu layout, text-input behavior, or editor panel composition to the engine 2D world renderer.

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

- Many game-world sprites render in batches.
- A tilemap renders through the engine 2D world renderer.
- Particle and parallax paths exist or are cleanly prepared.
- Sprite animation and 2D camera/world transform paths exist or are cleanly prepared.
- 2D debug drawing works without using `NikreonUI`.

## Phase 9: Game HUD and Menu Layer Using NikreonUI

Goal: compose in-game HUD and menus with `NikreonUI`. Do not create a separate HUD renderer.

Required widgets:

- Panels
- Buttons
- Text labels
- Icons
- Health/progress bars
- Inventory slots
- Crosshair
- Nine-slice panels
- Minimap placeholder
- Sliders
- Checkboxes
- Simple layout containers
- Anchors, margins, and padding
- Mouse hover and click
- Keyboard/controller navigation later

Important separation:

- `EditorUI` and `GameHUD` are separate composition layers that both use `NikreonUI`.
- `EditorUI` owns toolbar, hierarchy, inspector, console, asset browser, and editor viewport frame.
- `GameHUD` owns health bar, ammo, crosshair, inventory, pause menu, dialogue UI, minimap, and game settings.
- `GameHUD` appears in `Play`, exported/runtime game, and dedicated `HudEdit` mode.
- Normal `Edit` mode hides `GameHUD` by default.
- `Simulate` normally hides `GameHUD` unless the user explicitly previews it.
- `NikreonUI` handles UI layout, input, state, styles, text, icons, and widgets.
- The engine 2D world renderer remains responsible for sprites, tilemaps, particles, parallax, sprite animations, 2D cameras, and 2D debug drawing.

Viewport-local surface:

```cpp
struct UISurface {
    glm::vec2 origin;
    glm::vec2 size;
    float scale;
};
```

- For `GameHUD`, `{0, 0}` means the top-left of the game viewport/render target.
- In editor `Play` and `HudEdit`, render `GameHUD` into the editor viewport render target.
- In exported game mode, render `GameHUD` into the game window/swapchain.
- Keep the same `GameHUD` code in both cases by changing the `UISurface`, not the widget composition.

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

- `GameHUD` renders after world rendering and post-processing through `NikreonUI`.
- Basic interactive button state works.
- Crosshair, health bar, and text are drawn with `NikreonUI`.
- `GameHUD` appears in `Play` and `HudEdit`, remains hidden by default in `Edit`, and uses the same viewport-local layout in the exported/runtime game.

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

Goal: render the 3D and/or 2D game world to an offscreen framebuffer before `GameHUD`.

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
- `GameHUD` renders through `NikreonUI` after post-process.
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
- `GameHUD` layout and menu references later

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
- Engine 2D world sprite/quad shader
- `NikreonUI` UI image/icon and text shaders
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

Engine 2D world sprite shader inputs:

- Texture
- UV
- Tint
- Opacity
- Alpha blending

`NikreonUI` text shader inputs:

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

`NikreonUI`:

- Batch UI boxes, UI images/icons, and text.
- Keep `EditorUI` and `GameHUD` composition separate while sharing the UI library.
- Use viewport-local `UISurface` data for `GameHUD`.

Engine 2D world renderer:

- Batch sprites, tilemaps, and particles.
- Use atlas textures where possible.
- Use one index-buffer pattern for quads.
- Use dynamic vertex buffers or a ring buffer.
- Keep 2D camera/world transforms separate from UI-surface coordinates.

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
- Render 2D game-world sprites and tilemaps through the engine 2D world renderer.
- Render text using a font atlas.
- Render `EditorUI` through `NikreonUI`.
- Render `GameHUD` and menus through `NikreonUI`, without a separate HUD renderer.
- Render HUD elements such as health bar, crosshair, ammo text, inventory, pause menu, and dialogue UI.
- Show `GameHUD` in `Play`, exported/runtime game, and `HudEdit`, while hiding it by default in normal `Edit`.
- Provide `Edit`, `Play`, `Simulate`, and `HudEdit` viewport modes before complex renderer work depends on them.
- Keep `GameHUD` viewport-local so the same layout code renders into an editor viewport target or the exported game window/swapchain.
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
8. Finish `NikreonUI` foundation: labels, text input, scrollable clipped panels, responsive panel collapse/hide rules, UI images/icons, and nine-slice panels.
9. Add editor viewport integration and explicit `Edit`, `Play`, `Simulate`, and `HudEdit` modes before complex renderer work.
10. Add clean renderer architecture and separate `NikreonUI`, engine 2D world, 3D world, debug, and post-process responsibilities.
11. Add the engine 2D world renderer for sprites, tilemaps, particles, parallax layers, sprite animations, 2D camera/world transforms, and 2D debug drawing.
12. Compose `GameHUD` and menus through `NikreonUI` with viewport-local `UISurface` rendering. Do not create a separate HUD renderer.
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
[x] Phase 6  - Editor viewport integration
[ ] Phase 7  - Clean render architecture
[ ] Phase 8  - Engine 2D world renderer, sprites, and tilemaps
[ ] Phase 9  - Game HUD and menu layer using NikreonUI
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
