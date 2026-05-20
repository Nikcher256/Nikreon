#pragma once

#include "Engine/Editor/EditorUI.hpp"

namespace Engine {

class Renderer2D;

class EditorLayer {
public:
    void render(Renderer2D& renderer2D, const glm::uvec2& viewportSize);

private:
    EditorUI m_ui;
};

} // namespace Engine
