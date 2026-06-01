#include "Engine/Core/Layer.hpp"

namespace Engine {

// Provides a no-op update so simple render-only layers can override only rendering.
void Layer::onUpdate(const float deltaTime, const Input& input)
{
    (void)deltaTime;
    (void)input;
}

// Provides a no-op render so logic-only layers can override only updating.
void Layer::onRender(Renderer2D& renderer2D, TextRenderer& textRenderer, const glm::uvec2& viewportSize, const Input& input)
{
    (void)renderer2D;
    (void)textRenderer;
    (void)viewportSize;
    (void)input;
}

} // namespace Engine
