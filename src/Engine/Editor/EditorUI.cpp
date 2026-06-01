#include "Engine/Editor/EditorUI.hpp"

#include "Engine/Core/Input.hpp"
#include "Engine/Renderer/Renderer2D.hpp"
#include "Engine/Renderer/TextRenderer.hpp"
#include "Engine/UI/UIStyleParser.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>

#include <glm/vec4.hpp>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

namespace Engine {

#ifndef NIKREON_ASSET_DIR
#define NIKREON_ASSET_DIR "assets"
#endif

EditorUI::EditorUI()
    : m_playButton("toolbar.play")
    , m_pauseButton("toolbar.pause")
    , m_stopButton("toolbar.stop")
    , m_gridCheckbox("inspector.showGrid", true)
    , m_exposureSlider("inspector.exposure", 0.65f, 0.0f, 1.0f)
{
    std::string styleError;
    if (!UIStyleParser::loadFile(NIKREON_ASSET_DIR "/styles/editor.ui.css", m_style, styleError)) {
        spdlog::warn("Editor UI stylesheet was not loaded: {}", styleError);
    }

    m_hierarchyRows.reserve(5);
    for (int index = 0; index < 5; ++index) {
        m_hierarchyRows.emplace_back("hierarchy.row." + std::to_string(index));
    }

    m_playButton.setOnClick([this]() {
        m_runState = RunState::Playing;
        spdlog::info("Editor UI: Play button clicked.");
    });

    m_pauseButton.setOnClick([this]() {
        m_runState = RunState::Paused;
        spdlog::info("Editor UI: Pause button clicked.");
    });

    m_stopButton.setOnClick([this]() {
        m_runState = RunState::Stopped;
        m_viewportFocused = false;
        spdlog::info("Editor UI: Stop button clicked.");
    });

    m_gridCheckbox.setOnValueChanged([this](const bool enabled) {
        m_showGrid = enabled;
        spdlog::info("Editor UI: Viewport grid {}.", m_showGrid ? "enabled" : "disabled");
    });

    m_exposureSlider.setOnValueChanged([this](const float value) {
        m_previewExposure = value;
        if (std::abs(m_previewExposure - m_lastLoggedExposure) >= 0.05f) {
            m_lastLoggedExposure = m_previewExposure;
            spdlog::info("Editor UI: Preview brightness set to {:.2f}.", m_previewExposure);
        }
    });

    m_playButton.setStyleClass("toolbar");
    m_pauseButton.setStyleClass("toolbar");
    m_stopButton.setStyleClass("toolbar");
    m_exposureSlider.setStyleClass("inspector");

    for (std::size_t index = 0; index < m_hierarchyRows.size(); ++index) {
        m_hierarchyRows[index].setStyleClass("hierarchy-row");
        m_hierarchyRows[index].setOnClick([this, index]() {
            m_selectedHierarchyRow = static_cast<int>(index);
            spdlog::info("Editor UI: Hierarchy row {} selected.", index);
        });
    }
}

// Builds the native editor shell and lets widgets handle their own state.
void EditorUI::render(Renderer2D& renderer2D, TextRenderer& textRenderer, const glm::uvec2& viewportSize, const Input& input)
{
    m_context.beginFrame({
        input.mousePosition(),
        input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT),
    });

    const float width = static_cast<float>(std::max(viewportSize.x, 1U));
    const float height = static_cast<float>(std::max(viewportSize.y, 1U));
    layoutWidgets(width, height);
    updateWidgets();

    renderer2D.drawQuad({0.0f, 0.0f}, {width, height}, m_style.windowBackground);
    renderer2D.drawQuad({0.0f, 0.0f}, {width, m_toolbarHeight}, m_style.toolbar.fill);

    drawPanel(renderer2D, m_hierarchyBounds);
    drawPanel(renderer2D, m_inspectorBounds);
    drawPanel(renderer2D, m_consoleBounds);

    const float viewportBrightness = 0.035f + m_previewExposure * 0.045f;
    renderer2D.drawQuad(
        m_viewportBounds.position,
        m_viewportBounds.size,
        {viewportBrightness, viewportBrightness + 0.006f, viewportBrightness + 0.018f, 1.0f});

    const UIInteraction viewportInteraction = m_context.interact("viewport.main", m_viewportBounds.position, m_viewportBounds.size);
    if (viewportInteraction.pressed) {
        m_viewportFocused = true;
        spdlog::info("Editor UI: Viewport focused.");
    }

    if (m_showGrid) {
        drawViewportGrid(renderer2D, m_viewportBounds);
    }

    const glm::vec4 viewportBorder = m_viewportFocused ? m_style.viewportFocusedBorder : m_style.viewportBorder;
    renderer2D.drawRect(m_viewportBounds.position, m_viewportBounds.size, viewportBorder, 2.0f);

    renderWidgets(renderer2D);

    for (int index = 0; index < 4; ++index) {
        const float fieldY = m_exposureSlider.position().y + m_exposureSlider.size().y + 24.0f + static_cast<float>(index) * 42.0f;
        const glm::vec2 fieldPosition{m_inspectorBounds.position.x + 18.0f, fieldY};
        const glm::vec2 fieldSize{m_inspectorBounds.size.x - 36.0f, 28.0f};
        renderer2D.drawSdfRect(
            fieldPosition,
            fieldSize,
            m_style.field.borderRadius,
            m_style.field.fill,
            m_style.field.border,
            m_style.field.borderWidth);
    }

    renderLabels(textRenderer);
    m_context.endFrame();
}

// Computes responsive panel and widget bounds for the current framebuffer size.
void EditorUI::layoutWidgets(const float width, const float height)
{
    const float gap = m_style.gap;
    m_toolbarHeight = std::clamp(height * 0.07f, m_style.toolbarHeightMin, m_style.toolbarHeightMax);
    m_consoleHeight = std::clamp(height * 0.18f, 96.0f, 180.0f);

    const float availableWorkWidth = std::max(width - gap * 2.0f, 1.0f);
    const float leftPanelWidth = std::clamp(width * 0.15f, 180.0f, 260.0f);
    const float rightPanelWidth = std::clamp(width * 0.18f, 220.0f, 320.0f);

    UIDockLayout shellLayout;
    shellLayout.setBounds({{gap, m_toolbarHeight + gap}, {availableWorkWidth, std::max(0.0f, height - m_toolbarHeight - gap * 2.0f)}});
    shellLayout.setGap(gap);
    shellLayout.add(m_consoleBounds, UIDock::Bottom, m_consoleHeight);
    shellLayout.add(m_hierarchyBounds, UIDock::Left, std::min(leftPanelWidth, availableWorkWidth * 0.28f));
    shellLayout.add(m_inspectorBounds, UIDock::Right, std::min(rightPanelWidth, availableWorkWidth * 0.34f));
    shellLayout.add(m_viewportBounds, UIDock::Fill);
    shellLayout.layout();

    UILinearLayout toolbarLayout(UILayoutAxis::Horizontal);
    toolbarLayout.setBounds({{16.0f, 10.0f}, {std::max(0.0f, width - 32.0f), 28.0f}});
    toolbarLayout.setGap(12.0f);
    toolbarLayout.add(m_playButton, {76.0f, 28.0f});
    toolbarLayout.add(m_pauseButton, {76.0f, 28.0f});
    toolbarLayout.add(m_stopButton, {76.0f, 28.0f});
    toolbarLayout.layout();

    UILinearLayout hierarchyLayout(UILayoutAxis::Vertical);
    hierarchyLayout.setBounds(m_hierarchyBounds);
    hierarchyLayout.setPadding({16.0f, 38.0f, 16.0f, 20.0f});
    hierarchyLayout.setGap(12.0f);
    for (Button& row : m_hierarchyRows) {
        hierarchyLayout.add(row, {0.0f, 22.0f});
    }
    hierarchyLayout.layout();

    UIStackLayout inspectorLayout;
    inspectorLayout.setBounds(m_inspectorBounds);
    inspectorLayout.setPadding({18.0f, 42.0f, 18.0f, 0.0f});
    inspectorLayout.add(m_gridCheckbox, UIAnchors::fixed({0.0f, 0.0f}, {0.0f, 0.0f}, {22.0f, 22.0f}));
    inspectorLayout.add(m_exposureSlider, UIAnchors::horizontalStretch(54.0f, 24.0f));
    inspectorLayout.layout();
}

// Updates every retained widget after layout has assigned its bounds.
void EditorUI::updateWidgets()
{
    m_playButton.setSelected(m_runState == RunState::Playing);
    m_pauseButton.setSelected(m_runState == RunState::Paused);
    m_stopButton.setSelected(m_runState == RunState::Stopped);
    m_gridCheckbox.setChecked(m_showGrid);
    m_exposureSlider.setValue(m_previewExposure);

    m_playButton.update(m_context);
    m_pauseButton.update(m_context);
    m_stopButton.update(m_context);

    for (std::size_t index = 0; index < m_hierarchyRows.size(); ++index) {
        m_hierarchyRows[index].setSelected(static_cast<int>(index) == m_selectedHierarchyRow);
        m_hierarchyRows[index].update(m_context);
    }

    m_gridCheckbox.update(m_context);
    m_exposureSlider.update(m_context);
}

// Renders every retained widget and any editor-specific icon overlays.
void EditorUI::renderWidgets(Renderer2D& renderer2D)
{
    m_playButton.render(renderer2D, m_style);
    drawToolbarIcon(renderer2D, m_playButton.position(), m_playButton.size(), ToolbarIcon::Play, m_playButton.selected());
    m_pauseButton.render(renderer2D, m_style);
    drawToolbarIcon(renderer2D, m_pauseButton.position(), m_pauseButton.size(), ToolbarIcon::Pause, m_pauseButton.selected());
    m_stopButton.render(renderer2D, m_style);
    drawToolbarIcon(renderer2D, m_stopButton.position(), m_stopButton.size(), ToolbarIcon::Stop, m_stopButton.selected());

    for (const auto& row : m_hierarchyRows) {
        row.render(renderer2D, m_style);
    }

    m_gridCheckbox.render(renderer2D, m_style);
    m_exposureSlider.render(renderer2D, m_style);
}

// Draws the first real editor labels through the cached font atlas.
void EditorUI::renderLabels(TextRenderer& textRenderer)
{
    drawStyledText(textRenderer, "Hierarchy", {m_hierarchyBounds.position + glm::vec2{16.0f, 8.0f}, {m_hierarchyBounds.size.x - 32.0f, 18.0f}}, "heading");
    drawStyledText(textRenderer, "Inspector", {m_inspectorBounds.position + glm::vec2{18.0f, 8.0f}, {m_inspectorBounds.size.x - 36.0f, 18.0f}}, "heading");
    drawStyledText(textRenderer, "Console", {m_consoleBounds.position + glm::vec2{16.0f, 8.0f}, {m_consoleBounds.size.x - 32.0f, 18.0f}}, "heading");
    drawStyledText(textRenderer, "Viewport", {m_viewportBounds.position + glm::vec2{12.0f, 8.0f}, {m_viewportBounds.size.x - 24.0f, 18.0f}}, "muted", "viewport-title");

    constexpr std::array<std::string_view, 5> hierarchyNames = {
        "Main Camera",
        "Directional Light",
        "Environment",
        "Player",
        "UI Canvas",
    };

    for (std::size_t index = 0; index < m_hierarchyRows.size(); ++index) {
        drawStyledText(
            textRenderer,
            hierarchyNames[index],
            {m_hierarchyRows[index].position(), m_hierarchyRows[index].size()},
            "hierarchy-row");
    }

    drawStyledText(
        textRenderer,
        "Grid",
        {{m_gridCheckbox.position().x + 32.0f, m_gridCheckbox.position().y}, {m_inspectorBounds.size.x - 68.0f, m_gridCheckbox.size().y}},
        "control-label");
    drawStyledText(
        textRenderer,
        "Exposure",
        {{m_exposureSlider.position().x, m_exposureSlider.position().y - 22.0f}, {m_exposureSlider.size().x, 18.0f}},
        "control-label");
    drawStyledText(
        textRenderer,
        "Editor initialized. Text atlas rendering active.",
        {m_consoleBounds.position + glm::vec2{16.0f, 34.0f}, {m_consoleBounds.size.x - 32.0f, 18.0f}},
        "muted");
}

// Positions measured text inside a rectangle using the selected stylesheet class.
void EditorUI::drawStyledText(
    TextRenderer& textRenderer,
    const std::string_view text,
    const UIRect& bounds,
    const std::string_view styleClass,
    const std::string_view id)
{
    const UITextStyle& style = m_style.resolveText(styleClass, id);
    if (style.scale <= 0.0f || bounds.size.x <= 0.0f || bounds.size.y <= 0.0f) {
        return;
    }

    const glm::vec2 textSize = textRenderer.measureText(text, style.font, style.scale);
    glm::vec2 position = bounds.position;

    if (style.horizontalAlignment == UITextHorizontalAlignment::Center) {
        position.x += (bounds.size.x - textSize.x) * 0.5f;
    } else if (style.horizontalAlignment == UITextHorizontalAlignment::Right) {
        position.x += bounds.size.x - textSize.x;
    }

    if (style.verticalAlignment == UITextVerticalAlignment::Center) {
        position.y += (bounds.size.y - textSize.y) * 0.5f;
    } else if (style.verticalAlignment == UITextVerticalAlignment::Bottom) {
        position.y += bounds.size.y - textSize.y;
    }

    glm::vec4 color = style.color;
    color.a *= std::clamp(style.opacity, 0.0f, 1.0f);
    textRenderer.drawText(text, position + style.offset, color, style.font, style.scale);
}

// Draws a flat editor panel background and border.
void EditorUI::drawPanel(Renderer2D& renderer2D, const UIRect& bounds)
{
    renderer2D.drawSdfRect(
        bounds.position,
        bounds.size,
        m_style.panel.borderRadius,
        m_style.panel.fill,
        m_style.panel.border,
        m_style.panel.borderWidth);
}

// Draws simple toolbar glyphs from rectangles until text/icon rendering exists.
void EditorUI::drawToolbarIcon(
    Renderer2D& renderer2D,
    const glm::vec2& position,
    const glm::vec2& size,
    const ToolbarIcon icon,
    const bool selected)
{
    const glm::vec4 color = selected ? m_style.button.selectedIcon : m_style.button.icon;
    const glm::vec2 center{position.x + size.x * 0.5f, position.y + size.y * 0.5f};

    if (icon == ToolbarIcon::Play) {
        renderer2D.drawSdfRect({center.x - 7.0f, center.y - 8.0f}, {6.0f, 16.0f}, 1.5f, color, color, 0.0f);
        renderer2D.drawSdfRect({center.x - 1.0f, center.y - 5.0f}, {6.0f, 10.0f}, 1.5f, color, color, 0.0f);
        renderer2D.drawSdfRect({center.x + 5.0f, center.y - 2.0f}, {5.0f, 4.0f}, 1.5f, color, color, 0.0f);
    } else if (icon == ToolbarIcon::Pause) {
        renderer2D.drawSdfRect({center.x - 8.0f, center.y - 8.0f}, {5.0f, 16.0f}, 1.5f, color, color, 0.0f);
        renderer2D.drawSdfRect({center.x + 3.0f, center.y - 8.0f}, {5.0f, 16.0f}, 1.5f, color, color, 0.0f);
    } else {
        renderer2D.drawSdfRect({center.x - 8.0f, center.y - 8.0f}, {16.0f, 16.0f}, 2.0f, color, color, 0.0f);
    }
}

// Draws a lightweight viewport grid controlled by the inspector checkbox.
void EditorUI::drawViewportGrid(Renderer2D& renderer2D, const UIRect& bounds)
{
    constexpr float spacing = 40.0f;

    for (float x = bounds.position.x + spacing; x < bounds.position.x + bounds.size.x; x += spacing) {
        renderer2D.drawQuad({x, bounds.position.y}, {1.0f, bounds.size.y}, m_style.viewportGrid);
    }

    for (float y = bounds.position.y + spacing; y < bounds.position.y + bounds.size.y; y += spacing) {
        renderer2D.drawQuad({bounds.position.x, y}, {bounds.size.x, 1.0f}, m_style.viewportGrid);
    }
}

} // namespace Engine
