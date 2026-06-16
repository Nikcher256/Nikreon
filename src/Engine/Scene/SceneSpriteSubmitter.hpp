#pragma once

namespace Engine {

class SpriteRenderer;
class ResourceManager;
class Scene;

class SceneSpriteSubmitter final {
public:
    void submit(const Scene& scene, SpriteRenderer& spriteRenderer, ResourceManager& resources) const;
};

using Scene2DSubmitter = SceneSpriteSubmitter;

} // namespace Engine
