#include "Engine/Core/Layer.hpp"

namespace Engine {

// Provides a no-op update so simple render-only layers can override only rendering.
void Layer::onUpdate(const float deltaTime, const Input& input)
{
    (void)deltaTime;
    (void)input;
}

// Provides a no-op render so logic-only layers can override only updating.
void Layer::onRender(
    Renderer2D& renderer2D,
    Renderer2DWorld& renderer2DWorld,
    Renderer3D& renderer3D,
    DebugRenderer& debugRenderer,
    TextRenderer& textRenderer,
    ResourceManager& resources,
    const glm::uvec2& viewportSize,
    const Input& input)
{
    (void)renderer2D;
    (void)renderer2DWorld;
    (void)renderer3D;
    (void)debugRenderer;
    (void)textRenderer;
    (void)resources;
    (void)viewportSize;
    (void)input;
}

} // namespace Engine
