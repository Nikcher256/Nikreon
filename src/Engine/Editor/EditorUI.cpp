#include "Engine/Editor/EditorUI.hpp"

#include "Engine/Renderer/Renderer2D.hpp"

#include <algorithm>

#include <glm/vec4.hpp>

namespace Engine {

void EditorUI::render(Renderer2D& renderer2D, const glm::uvec2& viewportSize)
{
    const float width = static_cast<float>(std::max(viewportSize.x, 1U));
    const float height = static_cast<float>(std::max(viewportSize.y, 1U));

    constexpr float gap = 8.0f;
    const float toolbarHeight = std::clamp(height * 0.07f, 42.0f, 52.0f);
    const float consoleHeight = std::clamp(height * 0.18f, 96.0f, 180.0f);
    const float availableWorkWidth = std::max(width - gap * 4.0f, 1.0f);
    const float leftPanelWidth = std::clamp(width * 0.15f, 180.0f, 260.0f);
    const float rightPanelWidth = std::clamp(width * 0.18f, 220.0f, 320.0f);
    const float panelHeight = std::max(height - toolbarHeight - consoleHeight - gap * 3.0f, 1.0f);

    renderer2D.drawQuad({0.0f, 0.0f}, {width, height}, {0.07f, 0.075f, 0.09f, 1.0f});
    renderer2D.drawQuad({0.0f, 0.0f}, {width, toolbarHeight}, {0.10f, 0.11f, 0.14f, 1.0f});

    float buttonX = 16.0f;
    drawButton(renderer2D, {buttonX, 10.0f}, {76.0f, 28.0f}, true);
    buttonX += 88.0f;
    drawButton(renderer2D, {buttonX, 10.0f}, {76.0f, 28.0f});
    buttonX += 88.0f;
    drawButton(renderer2D, {buttonX, 10.0f}, {76.0f, 28.0f});

    const glm::vec2 hierarchyPosition{gap, toolbarHeight + gap};
    const glm::vec2 hierarchySize{std::min(leftPanelWidth, availableWorkWidth * 0.28f), panelHeight};
    drawPanel(renderer2D, hierarchyPosition, hierarchySize);

    const glm::vec2 inspectorSize{std::min(rightPanelWidth, availableWorkWidth * 0.34f), panelHeight};
    const glm::vec2 inspectorPosition{width - inspectorSize.x - gap, toolbarHeight + gap};
    drawPanel(renderer2D, inspectorPosition, inspectorSize);

    const glm::vec2 consolePosition{gap, height - consoleHeight - gap};
    const glm::vec2 consoleSize{width - gap * 2.0f, consoleHeight};
    drawPanel(renderer2D, consolePosition, consoleSize);

    const glm::vec2 viewportPosition{hierarchyPosition.x + hierarchySize.x + gap, toolbarHeight + gap};
    const glm::vec2 viewportSizePixels{
        inspectorPosition.x - viewportPosition.x - gap,
        panelHeight,
    };

    renderer2D.drawQuad(viewportPosition, viewportSizePixels, {0.045f, 0.05f, 0.065f, 1.0f});
    renderer2D.drawRect(viewportPosition, viewportSizePixels, {0.25f, 0.32f, 0.42f, 1.0f}, 2.0f);

    for (int index = 0; index < 5; ++index) {
        const float rowY = hierarchyPosition.y + 20.0f + static_cast<float>(index) * 34.0f;
        renderer2D.drawQuad({hierarchyPosition.x + 16.0f, rowY}, {hierarchySize.x - 32.0f, 22.0f}, {0.15f, 0.17f, 0.21f, 1.0f});
    }

    for (int index = 0; index < 4; ++index) {
        const float fieldY = inspectorPosition.y + 22.0f + static_cast<float>(index) * 42.0f;
        renderer2D.drawQuad({inspectorPosition.x + 18.0f, fieldY}, {inspectorSize.x - 36.0f, 28.0f}, {0.13f, 0.145f, 0.18f, 1.0f});
        renderer2D.drawRect({inspectorPosition.x + 18.0f, fieldY}, {inspectorSize.x - 36.0f, 28.0f}, {0.22f, 0.26f, 0.32f, 1.0f}, 1.0f);
    }
}

void EditorUI::drawPanel(Renderer2D& renderer2D, const glm::vec2& position, const glm::vec2& size)
{
    renderer2D.drawQuad(position, size, {0.095f, 0.105f, 0.13f, 1.0f});
    renderer2D.drawRect(position, size, {0.20f, 0.23f, 0.29f, 1.0f}, 1.0f);
}

void EditorUI::drawButton(Renderer2D& renderer2D, const glm::vec2& position, const glm::vec2& size, const bool active)
{
    const glm::vec4 fill = active ? glm::vec4{0.20f, 0.48f, 0.82f, 1.0f} : glm::vec4{0.16f, 0.18f, 0.23f, 1.0f};
    const glm::vec4 border = active ? glm::vec4{0.44f, 0.70f, 1.0f, 1.0f} : glm::vec4{0.26f, 0.30f, 0.38f, 1.0f};
    renderer2D.drawQuad(position, size, fill);
    renderer2D.drawRect(position, size, border, 1.0f);
}

} // namespace Engine
