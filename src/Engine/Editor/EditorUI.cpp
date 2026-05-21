#include "Engine/Editor/EditorUI.hpp"

#include "Engine/Renderer/Renderer2D.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include <glm/vec4.hpp>
#include <spdlog/spdlog.h>

namespace Engine {

// Builds the native editor shell and routes widget input through UIContext.
void EditorUI::render(Renderer2D& renderer2D, const glm::uvec2& viewportSize, const Input& input)
{
    m_context.beginFrame(input);

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
    const glm::vec2 playButtonPosition{buttonX, 10.0f};
    const glm::vec2 toolbarButtonSize{76.0f, 28.0f};
    const UIInteraction playButton = m_context.interact("toolbar.play", playButtonPosition, toolbarButtonSize);
    if (playButton.pressed) {
        m_runState = RunState::Playing;
        spdlog::info("Editor UI: Play button clicked.");
    }
    drawButton(renderer2D, playButtonPosition, toolbarButtonSize, playButton, m_runState == RunState::Playing);
    drawToolbarIcon(renderer2D, playButtonPosition, toolbarButtonSize, ToolbarIcon::Play, m_runState == RunState::Playing);

    buttonX += 88.0f;
    const glm::vec2 pauseButtonPosition{buttonX, 10.0f};
    const UIInteraction pauseButton = m_context.interact("toolbar.pause", pauseButtonPosition, toolbarButtonSize);
    if (pauseButton.pressed) {
        m_runState = RunState::Paused;
        spdlog::info("Editor UI: Pause button clicked.");
    }
    drawButton(renderer2D, pauseButtonPosition, toolbarButtonSize, pauseButton, m_runState == RunState::Paused);
    drawToolbarIcon(renderer2D, pauseButtonPosition, toolbarButtonSize, ToolbarIcon::Pause, m_runState == RunState::Paused);

    buttonX += 88.0f;
    const glm::vec2 stopButtonPosition{buttonX, 10.0f};
    const UIInteraction stopButton = m_context.interact("toolbar.stop", stopButtonPosition, toolbarButtonSize);
    if (stopButton.pressed) {
        m_runState = RunState::Stopped;
        m_viewportFocused = false;
        spdlog::info("Editor UI: Stop button clicked.");
    }
    drawButton(renderer2D, stopButtonPosition, toolbarButtonSize, stopButton, m_runState == RunState::Stopped);
    drawToolbarIcon(renderer2D, stopButtonPosition, toolbarButtonSize, ToolbarIcon::Stop, m_runState == RunState::Stopped);

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

    const float viewportBrightness = 0.035f + m_previewExposure * 0.045f;
    renderer2D.drawQuad(viewportPosition, viewportSizePixels, {viewportBrightness, viewportBrightness + 0.006f, viewportBrightness + 0.018f, 1.0f});
    const UIInteraction viewportInteraction = m_context.interact("viewport.main", viewportPosition, viewportSizePixels);
    if (viewportInteraction.pressed) {
        m_viewportFocused = true;
        spdlog::info("Editor UI: Viewport focused.");
    }

    if (m_showGrid) {
        drawViewportGrid(renderer2D, viewportPosition, viewportSizePixels);
    }

    const glm::vec4 viewportBorder = m_viewportFocused
        ? glm::vec4{0.32f, 0.58f, 0.88f, 1.0f}
        : glm::vec4{0.25f, 0.32f, 0.42f, 1.0f};
    renderer2D.drawRect(viewportPosition, viewportSizePixels, viewportBorder, 2.0f);

    for (int index = 0; index < 5; ++index) {
        const float rowY = hierarchyPosition.y + 20.0f + static_cast<float>(index) * 34.0f;
        const glm::vec2 rowPosition{hierarchyPosition.x + 16.0f, rowY};
        const glm::vec2 rowSize{hierarchySize.x - 32.0f, 22.0f};
        const std::string rowId = "hierarchy.row." + std::to_string(index);
        const UIInteraction row = m_context.interact(rowId, rowPosition, rowSize);
        if (row.pressed) {
            m_selectedHierarchyRow = index;
            spdlog::info("Editor UI: Hierarchy row {} selected.", index);
        }

        const bool selected = m_selectedHierarchyRow == index;
        const glm::vec4 rowColor = selected
            ? glm::vec4{0.20f, 0.32f, 0.48f, 1.0f}
            : row.hovered
                ? glm::vec4{0.17f, 0.19f, 0.24f, 1.0f}
                : glm::vec4{0.15f, 0.17f, 0.21f, 1.0f};
        renderer2D.drawQuad(rowPosition, rowSize, rowColor);
    }

    const glm::vec2 checkboxPosition{inspectorPosition.x + 18.0f, inspectorPosition.y + 22.0f};
    const glm::vec2 checkboxSize{22.0f, 22.0f};
    const UIInteraction checkbox = m_context.interact("inspector.showGrid", checkboxPosition, checkboxSize);
    if (checkbox.pressed) {
        m_showGrid = !m_showGrid;
        spdlog::info("Editor UI: Viewport grid {}.", m_showGrid ? "enabled" : "disabled");
    }
    drawCheckbox(renderer2D, checkboxPosition, checkboxSize, m_showGrid, checkbox);

    const glm::vec2 sliderPosition{inspectorPosition.x + 18.0f, inspectorPosition.y + 64.0f};
    const glm::vec2 sliderSize{inspectorSize.x - 36.0f, 24.0f};
    const UIInteraction slider = m_context.interact("inspector.exposure", sliderPosition, sliderSize);
    if (slider.held) {
        const float t = std::clamp((m_context.mousePosition().x - sliderPosition.x) / std::max(sliderSize.x, 1.0f), 0.0f, 1.0f);
        m_previewExposure = t;
        if (std::abs(m_previewExposure - m_lastLoggedExposure) >= 0.05f) {
            m_lastLoggedExposure = m_previewExposure;
            spdlog::info("Editor UI: Preview brightness set to {:.2f}.", m_previewExposure);
        }
    }
    drawSlider(renderer2D, sliderPosition, sliderSize, m_previewExposure, 0.0f, 1.0f, slider);

    for (int index = 0; index < 4; ++index) {
        const float fieldY = inspectorPosition.y + 112.0f + static_cast<float>(index) * 42.0f;
        renderer2D.drawQuad({inspectorPosition.x + 18.0f, fieldY}, {inspectorSize.x - 36.0f, 28.0f}, {0.13f, 0.145f, 0.18f, 1.0f});
        renderer2D.drawRect({inspectorPosition.x + 18.0f, fieldY}, {inspectorSize.x - 36.0f, 28.0f}, {0.22f, 0.26f, 0.32f, 1.0f}, 1.0f);
    }

    m_context.endFrame();
}

// Draws a flat editor panel background and border.
void EditorUI::drawPanel(Renderer2D& renderer2D, const glm::vec2& position, const glm::vec2& size)
{
    renderer2D.drawQuad(position, size, {0.095f, 0.105f, 0.13f, 1.0f});
    renderer2D.drawRect(position, size, {0.20f, 0.23f, 0.29f, 1.0f}, 1.0f);
}

// Draws a button using interaction state from UIContext.
void EditorUI::drawButton(
    Renderer2D& renderer2D,
    const glm::vec2& position,
    const glm::vec2& size,
    const UIInteraction& state,
    const bool selected)
{
    const glm::vec4 fill = selected
        ? glm::vec4{0.20f, 0.48f, 0.82f, 1.0f}
        : state.held
            ? glm::vec4{0.10f, 0.14f, 0.20f, 1.0f}
            : state.hovered
                ? glm::vec4{0.20f, 0.23f, 0.30f, 1.0f}
                : glm::vec4{0.16f, 0.18f, 0.23f, 1.0f};
    const glm::vec4 border = selected ? glm::vec4{0.44f, 0.70f, 1.0f, 1.0f} : glm::vec4{0.26f, 0.30f, 0.38f, 1.0f};
    renderer2D.drawQuad(position, size, fill);
    renderer2D.drawRect(position, size, border, 1.0f);

    if (selected) {
        renderer2D.drawQuad({position.x, position.y + size.y - 4.0f}, {size.x, 4.0f}, {0.52f, 0.78f, 1.0f, 1.0f});
    }
}

// Draws simple toolbar glyphs from rectangles until text/icon rendering exists.
void EditorUI::drawToolbarIcon(
    Renderer2D& renderer2D,
    const glm::vec2& position,
    const glm::vec2& size,
    const ToolbarIcon icon,
    const bool selected)
{
    const glm::vec4 color = selected ? glm::vec4{0.92f, 0.98f, 1.0f, 1.0f} : glm::vec4{0.56f, 0.62f, 0.72f, 1.0f};
    const glm::vec2 center{position.x + size.x * 0.5f, position.y + size.y * 0.5f};

    if (icon == ToolbarIcon::Play) {
        renderer2D.drawQuad({center.x - 7.0f, center.y - 8.0f}, {6.0f, 16.0f}, color);
        renderer2D.drawQuad({center.x - 1.0f, center.y - 5.0f}, {6.0f, 10.0f}, color);
        renderer2D.drawQuad({center.x + 5.0f, center.y - 2.0f}, {5.0f, 4.0f}, color);
    } else if (icon == ToolbarIcon::Pause) {
        renderer2D.drawQuad({center.x - 8.0f, center.y - 8.0f}, {5.0f, 16.0f}, color);
        renderer2D.drawQuad({center.x + 3.0f, center.y - 8.0f}, {5.0f, 16.0f}, color);
    } else {
        renderer2D.drawQuad({center.x - 8.0f, center.y - 8.0f}, {16.0f, 16.0f}, color);
    }
}

// Draws a checkbox with a filled mark when enabled.
void EditorUI::drawCheckbox(
    Renderer2D& renderer2D,
    const glm::vec2& position,
    const glm::vec2& size,
    const bool checked,
    const UIInteraction& state)
{
    const glm::vec4 fill = state.hovered ? glm::vec4{0.18f, 0.21f, 0.27f, 1.0f} : glm::vec4{0.13f, 0.145f, 0.18f, 1.0f};
    renderer2D.drawQuad(position, size, fill);
    renderer2D.drawRect(position, size, {0.28f, 0.33f, 0.42f, 1.0f}, 1.0f);

    if (checked) {
        renderer2D.drawQuad({position.x + 5.0f, position.y + 5.0f}, {size.x - 10.0f, size.y - 10.0f}, {0.30f, 0.62f, 0.95f, 1.0f});
    }
}

// Draws a simple horizontal slider track and draggable fill amount.
void EditorUI::drawSlider(
    Renderer2D& renderer2D,
    const glm::vec2& position,
    const glm::vec2& size,
    const float value,
    const float minValue,
    const float maxValue,
    const UIInteraction& state)
{
    const float t = maxValue > minValue ? std::clamp((value - minValue) / (maxValue - minValue), 0.0f, 1.0f) : 0.0f;
    const glm::vec4 track = state.hovered || state.held ? glm::vec4{0.18f, 0.21f, 0.27f, 1.0f} : glm::vec4{0.13f, 0.145f, 0.18f, 1.0f};
    renderer2D.drawQuad(position, size, track);
    renderer2D.drawQuad(position, {size.x * t, size.y}, {0.30f, 0.52f, 0.78f, 1.0f});
    renderer2D.drawRect(position, size, {0.24f, 0.29f, 0.36f, 1.0f}, 1.0f);
}

// Draws a lightweight viewport grid controlled by the inspector checkbox.
void EditorUI::drawViewportGrid(Renderer2D& renderer2D, const glm::vec2& position, const glm::vec2& size)
{
    constexpr float spacing = 40.0f;
    const glm::vec4 gridColor{0.10f, 0.13f, 0.17f, 0.8f};

    for (float x = position.x + spacing; x < position.x + size.x; x += spacing) {
        renderer2D.drawQuad({x, position.y}, {1.0f, size.y}, gridColor);
    }

    for (float y = position.y + spacing; y < position.y + size.y; y += spacing) {
        renderer2D.drawQuad({position.x, y}, {size.x, 1.0f}, gridColor);
    }
}

} // namespace Engine
