#pragma once

#include <glm/vec2.hpp>

namespace Engine {

class Input;
class Renderer2D;
class Renderer2DWorld;
class TextRenderer;

class Layer {
public:
    virtual ~Layer() = default;

    virtual void onUpdate(float deltaTime, const Input& input);
    virtual void onRender(
        Renderer2D& renderer2D,
        Renderer2DWorld& renderer2DWorld,
        TextRenderer& textRenderer,
        const glm::uvec2& viewportSize,
        const Input& input);
};

} // namespace Engine
