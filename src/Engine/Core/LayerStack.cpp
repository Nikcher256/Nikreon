#include "Engine/Core/LayerStack.hpp"

#include "Engine/Core/Layer.hpp"

namespace Engine {

// Adds a layer to the end of the stack so it updates and renders after earlier layers.
void LayerStack::pushLayer(std::unique_ptr<Layer> layer)
{
    m_layers.push_back(std::move(layer));
}

// Updates layers in stack order.
void LayerStack::update(const float deltaTime, const Input& input)
{
    for (const auto& layer : m_layers) {
        layer->onUpdate(deltaTime, input);
    }
}

// Renders layers in stack order.
void LayerStack::render(Renderer2D& renderer2D, TextRenderer& textRenderer, const glm::uvec2& viewportSize, const Input& input)
{
    for (const auto& layer : m_layers) {
        layer->onRender(renderer2D, textRenderer, viewportSize, input);
    }
}

} // namespace Engine
