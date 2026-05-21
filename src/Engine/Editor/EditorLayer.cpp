#include "Engine/Editor/EditorLayer.hpp"

namespace Engine {

// Updates editor-owned state before rendering.
void EditorLayer::onUpdate(const float deltaTime, const Input& input)
{
    (void)deltaTime;
    (void)input;
}

// Draws the editor layer after the renderer has started a frame.
void EditorLayer::onRender(Renderer2D& renderer2D, const glm::uvec2& viewportSize, const Input& input)
{
    m_ui.render(renderer2D, viewportSize, input);
}

} // namespace Engine
