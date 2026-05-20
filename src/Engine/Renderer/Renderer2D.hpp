#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace Engine {

class Renderer2D {
public:
    virtual ~Renderer2D() = default;

    virtual void begin(const glm::uvec2& viewportSize) = 0;
    virtual void drawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color) = 0;
    virtual void drawRect(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color, float thickness = 1.0f) = 0;
    virtual void end() = 0;
};

} // namespace Engine
