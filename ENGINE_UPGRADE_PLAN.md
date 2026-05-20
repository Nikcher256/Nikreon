# Modern Vulkan Game Engine Upgrade Plan

This document is the working roadmap for upgrading the C++ Vulkan engine into a modular modern game engine. It is intentionally phased: each phase should compile, preserve existing working behavior where possible, and avoid rewriting unrelated systems.

The main target is a renderer and engine architecture that can load Blender-exported GLB/glTF assets with proper materials, textures, lighting, transparency, shadows, and later animation, while also supporting a real GPU-driven 2D/UI/HUD pipeline.

## Guiding Rules

- Keep existing working code unless a change is needed for the current phase.
- Do not build one huge renderer file. Split responsibility into small engine modules.
- Every phase should build before moving on.
- Prefer glTF/GLB as the primary Blender import path.
- HUD, menus, text, sprites, debug overlays, and editor overlays are not random 3D objects. They render through the 2D/UI/debug systems after the 3D world and post-processing.
- ImGui may be used for editor/debug tools if already present, but final in-game HUD should be engine-native and GPU-rendered.
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
    Gizmos
    Picking
    Selection
```

## Phase 1: Clean Render Architecture

Goal: introduce the architecture and render pass order without breaking existing rendering.

Tasks:

- Add or prepare the main renderer orchestration layer.
- Add empty or minimal modules for `Renderer3D`, `Renderer2D`, `TextRenderer`, `UIRenderer`, `DebugRenderer`, `ShadowRenderer`, `SkyboxRenderer`, and `PostProcessRenderer`.
- Define a frame render sequence matching the target render order.
- Add clear interfaces for per-frame begin/end, resize, resource cleanup, and command buffer recording.
- Prepare placeholders for screen-space UI, world-space UI, billboards, editor overlays, and debug labels.

Build gate:

- Existing scene still renders.
- New modules compile even if most are placeholders.
- No Vulkan resources are leaked or recreated per frame unnecessarily.

## Phase 2: Renderer2D, Sprites, and Quads

Goal: add a real batched GPU 2D renderer.

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

## Phase 3: Text Rendering

Goal: render GPU text through font atlas glyph quads.

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
- Text can be drawn in HUD/debug space.

## Phase 4: UI and HUD Layer

Goal: build engine-native UI on top of `Renderer2D` and `TextRenderer`.

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
- Crosshair, health bar, and text can be drawn without ImGui.

## Phase 5: Debug Renderer, Editor Overlays, and Gizmo Prep

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

## Phase 6: Modern Model Loading

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

## Phase 7: Material System and PBR Basics

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

## Phase 8: Lighting System

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

## Phase 9: Shadows

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

## Phase 10: Post-Processing

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

## Phase 11: Scene, Entity, and Component Cleanup

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

## Phase 12: Physics, Raycasting, and Picking Preparation

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

## Phase 13: Audio and 3D Audio

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

## Phase 14: Scripting Preparation

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

## Phase 15: Editor Gizmos and Overlays

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

## Phase 16: Animation and Bones Preparation

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

## Phase 17: Resource Management

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

## Phase 18: Scene Saving and Loading

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

## Phase 19: Multiple Render Styles

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

## Phase 20: Shader Organization

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

## Phase 21: Performance Rules

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

1. Clean renderer architecture and render pass order.
2. Renderer2D with batched quads and sprites.
3. TextRenderer with font atlas.
4. UI/HUD layer using Renderer2D/TextRenderer.
5. DebugRenderer for lines, boxes, and labels.
6. Upgrade model loading for GLB/glTF multiple meshes/materials.
7. Material system and PBR shader basics.
8. Lighting system.
9. Shadows.
10. Post-processing.
11. Scene/entity/component cleanup.
12. Physics/raycast/picking preparation.
13. Audio/3D audio.
14. Scripting preparation.
15. Editor gizmos and overlays.

## Current Phase Tracker

Update this section as work progresses.

```text
[ ] Phase 1  - Clean render architecture
[ ] Phase 2  - Renderer2D batching
[ ] Phase 3  - Text rendering
[ ] Phase 4  - UI/HUD system
[ ] Phase 5  - Debug renderer
[ ] Phase 6  - GLB/glTF model loading
[ ] Phase 7  - Material/PBR system
[ ] Phase 8  - Lighting
[ ] Phase 9  - Shadows
[ ] Phase 10 - Post-processing
[ ] Phase 11 - Scene/components
[ ] Phase 12 - Physics/raycast/picking
[ ] Phase 13 - Audio/3D audio
[ ] Phase 14 - Scripting
[ ] Phase 15 - Editor gizmos/overlays
[ ] Phase 16 - Animation preparation
[ ] Phase 17 - Resource management
[ ] Phase 18 - Scene serialization
[ ] Phase 19 - Multiple render styles
[ ] Phase 20 - Shader organization
[ ] Phase 21 - Performance audit
```

## Notes for Future Edits

- If a later implementation discovers a better order, update this plan instead of forcing the engine into the original sequence.
- If an existing module already covers a responsibility, evolve it rather than duplicating it.
- Each completed phase should add a short note describing what changed, what was deferred, and how it was tested.
