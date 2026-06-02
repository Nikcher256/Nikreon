#include "Engine/Editor/EditorLayer.hpp"

namespace Engine {

// Updates editor-owned state before rendering.
void EditorLayer::onUpdate(const float deltaTime, const Input& input)
{
    m_ui.updateEditorCamera(deltaTime, input);
}

const EditorViewport& EditorLayer::viewport() const
{
    return m_ui.viewport();
}

// Draws the editor layer after the renderer has started a frame.
void EditorLayer::onRender(Renderer2D& renderer2D, TextRenderer& textRenderer, const glm::uvec2& viewportSize, const Input& input)
{
    m_ui.render(renderer2D, textRenderer, viewportSize, input);
}

} // namespace Engine
