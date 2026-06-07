#pragma once

#include <memory>
#include <vector>

#include <glm/vec2.hpp>

namespace Engine {

class ResourceManager;
class Input;
class Layer;
class Renderer2D;
class Renderer2DWorld;
class TextRenderer;

class LayerStack {
public:
    void pushLayer(std::unique_ptr<Layer> layer);
    void update(float deltaTime, const Input& input);
    void render(
        Renderer2D& renderer2D,
        Renderer2DWorld& renderer2DWorld,
        TextRenderer& textRenderer,
        ResourceManager& resources,
        const glm::uvec2& viewportSize,
        const Input& input);

private:
    std::vector<std::unique_ptr<Layer>> m_layers;
};

} // namespace Engine
