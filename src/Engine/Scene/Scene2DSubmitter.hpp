#pragma once

namespace Engine {

class Renderer2DWorld;
class ResourceManager;
class Scene;

class Scene2DSubmitter final {
public:
    void submit(const Scene& scene, Renderer2DWorld& renderer2DWorld, ResourceManager& resources) const;
};

} // namespace Engine