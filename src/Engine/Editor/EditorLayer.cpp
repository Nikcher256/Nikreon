#include "Engine/Editor/EditorLayer.hpp"

namespace Engine {

void EditorLayer::render(Renderer2D& renderer2D, const glm::uvec2& viewportSize)
{
    m_ui.render(renderer2D, viewportSize);
}

} // namespace Engine
