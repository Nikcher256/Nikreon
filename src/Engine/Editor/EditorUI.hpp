#pragma once

#include <glm/vec2.hpp>

namespace Engine {

class Renderer2D;

class EditorUI {
public:
    void render(Renderer2D& renderer2D, const glm::uvec2& viewportSize);

private:
    void drawPanel(Renderer2D& renderer2D, const glm::vec2& position, const glm::vec2& size);
    void drawButton(Renderer2D& renderer2D, const glm::vec2& position, const glm::vec2& size, bool active = false);
};

} // namespace Engine
