#include "Engine/Editor/EditorUI.hpp"

#include "Engine/Core/FileDialog.hpp"
#include "Engine/Core/Input.hpp"
#include "Engine/Renderer/Renderer2D.hpp"
#include "Engine/Renderer/Renderer2DWorld.hpp"
#include "Engine/Renderer/TextRenderer.hpp"
#include "Engine/UI/UIStyleParser.hpp"
#include "Engine/UI/UIFrame.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <functional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec4.hpp>
#include <glm/common.hpp>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

namespace Engine {

#ifndef NIKREON_ASSET_DIR
#define NIKREON_ASSET_DIR "assets"
#endif

std::vector<UIKey> uiKeys(const Input& input)
{
    std::vector<UIKey> keys;
    const bool controlDown = input.isKeyPressed(GLFW_KEY_LEFT_CONTROL) || input.isKeyPressed(GLFW_KEY_RIGHT_CONTROL);
    for (const int key : input.pressedKeys()) {
        if (controlDown) {
            switch (key) {
            case GLFW_KEY_A: keys.push_back(UIKey::SelectAll); continue;
            case GLFW_KEY_C: keys.push_back(UIKey::Copy); continue;
            case GLFW_KEY_V: keys.push_back(UIKey::Paste); continue;
            default: break;
            }
        }
        switch (key) {
        case GLFW_KEY_BACKSPACE: keys.push_back(UIKey::Backspace); break;
        case GLFW_KEY_DELETE: keys.push_back(UIKey::Delete); break;
        case GLFW_KEY_LEFT: keys.push_back(UIKey::Left); break;
        case GLFW_KEY_RIGHT: keys.push_back(UIKey::Right); break;
        case GLFW_KEY_HOME: keys.push_back(UIKey::Home); break;
        case GLFW_KEY_END: keys.push_back(UIKey::End); break;
        case GLFW_KEY_ENTER: keys.push_back(UIKey::Enter); break;
        case GLFW_KEY_ESCAPE: keys.push_back(UIKey::Escape); break;
        default: break;
        }
    }
    return keys;
}

EditorUI::EditorUI()
    : m_playButton("toolbar.play")
    , m_pauseButton("toolbar.pause")
    , m_stopButton("toolbar.stop")
    , m_toggleHierarchyButton("toolbar.toggleHierarchy")
    , m_toggleInspectorButton("toolbar.toggleInspector")
    , m_toggleConsoleButton("toolbar.toggleConsole")
    , m_editModeButton("toolbar.mode.edit")
    , m_playModeButton("toolbar.mode.play")
    , m_simulateModeButton("toolbar.mode.simulate")
    , m_hudEditModeButton("toolbar.mode.hudEdit")
    , m_addSpriteButton("world2d.addSprite")
    , m_addManySpritesButton("world2d.addManySprites")
    , m_addTilemapButton("world2d.addTilemap")
    , m_animateSpriteButton("world2d.animateSprite")
    , m_toggleParallaxButton("world2d.toggleParallax")
    , m_toggleParticlesButton("world2d.toggleParticles")
    , m_toggleDebugShapesButton("world2d.toggleDebugShapes")
    , m_clearWorldTestButton("world2d.clear")
    , m_gridCheckbox("inspector.showGrid", true)
    , m_clearColorPicker("inspector.clearColor", m_viewportClearColor)
    , m_exposureSlider("inspector.exposure", 0.65f, 0.0f, 1.0f)
    , m_lightIntensityInput("inspector.lightIntensity", 4.0f, 0.0f, 100.0f)
    , m_positionXInput("inspector.position.x", 12.5f, -100.0f, 100.0f)
    , m_positionYInput("inspector.position.y", -4.0f, -100.0f, 100.0f)
    , m_positionZInput("inspector.position.z", 8.0f, -100.0f, 100.0f)
    , m_objectNameInput("inspector.objectName", "Directional Light")
    , m_spritePathInput("world2d.spritePath", "assets/sprites/test.png")
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
        m_viewport.setMode(EditorViewportMode::Play);
        spdlog::info("Editor UI: Play button clicked.");
    });

    m_pauseButton.setOnClick([this]() {
        m_runState = RunState::Paused;
        spdlog::info("Editor UI: Pause button clicked.");
    });

    m_stopButton.setOnClick([this]() {
        m_runState = RunState::Stopped;
        m_viewport.setMode(EditorViewportMode::Edit);
        spdlog::info("Editor UI: Stop button clicked.");
    });

    m_gridCheckbox.setOnValueChanged([this](const bool enabled) {
        m_showGrid = enabled;
        spdlog::info("Editor UI: Viewport grid {}.", m_showGrid ? "enabled" : "disabled");
    });
    m_clearColorPicker.setOnColorChanged([this](const glm::vec4& color) {
        m_viewportClearColor = color;
    });

    m_exposureSlider.setOnValueChanged([this](const float value) {
        m_previewExposure = value;
        if (std::abs(m_previewExposure - m_lastLoggedExposure) >= 0.05f) {
            m_lastLoggedExposure = m_previewExposure;
            spdlog::info("Editor UI: Preview brightness set to {:.2f}.", m_previewExposure);
        }
    });

    m_lightIntensityInput.setSensitivity(0.05f);
    m_lightIntensityInput.setPrecision(2);
    m_lightIntensityInput.setOnValueChanged([this](const float value) {
        m_lightIntensity = value;
    });
    m_positionXInput.setSensitivity(0.1f);
    m_positionYInput.setSensitivity(0.1f);
    m_positionZInput.setSensitivity(0.1f);
    m_positionXInput.setPrecision(2);
    m_positionYInput.setPrecision(2);
    m_positionZInput.setPrecision(2);
    m_positionXInput.setOnValueChanged([this](const float value) { m_previewPosition.x = value; });
    m_positionYInput.setOnValueChanged([this](const float value) { m_previewPosition.y = value; });
    m_positionZInput.setOnValueChanged([this](const float value) { m_previewPosition.z = value; });
    m_objectNameInput.setPlaceholder("Object name");

    m_playButton.setStyleClass("toolbar");
    m_pauseButton.setStyleClass("toolbar");
    m_stopButton.setStyleClass("toolbar");
    m_toggleHierarchyButton.setStyleClass("toolbar-toggle");
    m_toggleInspectorButton.setStyleClass("toolbar-toggle");
    m_toggleConsoleButton.setStyleClass("toolbar-toggle");
    m_editModeButton.setStyleClass("toolbar");
    m_playModeButton.setStyleClass("toolbar");
    m_simulateModeButton.setStyleClass("toolbar");
    m_hudEditModeButton.setStyleClass("toolbar");
    m_addSpriteButton.setStyleClass("toolbar");
    m_addManySpritesButton.setStyleClass("toolbar");
    m_addTilemapButton.setStyleClass("toolbar");
    m_animateSpriteButton.setStyleClass("toolbar");
    m_toggleParallaxButton.setStyleClass("toolbar");
    m_toggleParticlesButton.setStyleClass("toolbar");
    m_toggleDebugShapesButton.setStyleClass("toolbar");
    m_clearWorldTestButton.setStyleClass("toolbar");
    m_exposureSlider.setStyleClass("inspector");
    m_lightIntensityInput.setStyleClass("inspector");
    m_positionXInput.setStyleClass("plain");
    m_positionYInput.setStyleClass("plain");
    m_positionZInput.setStyleClass("plain");
    m_objectNameInput.setStyleClass("inspector");
    m_spritePathInput.setInputStyleClass("inspector");
    m_spritePathInput.setButtonStyleClass("toolbar");
    m_spritePathInput.setButtonLabel("Browse");
    m_spritePathInput.setBrowseButtonWidth(78.0f);

    m_toggleHierarchyButton.setOnClick([this]() { m_hierarchyCollapsed = !m_hierarchyCollapsed; });
    m_toggleInspectorButton.setOnClick([this]() { m_inspectorCollapsed = !m_inspectorCollapsed; });
    m_toggleConsoleButton.setOnClick([this]() { m_consoleCollapsed = !m_consoleCollapsed; });
    m_editModeButton.setOnClick([this]() {
        m_viewport.setMode(EditorViewportMode::Edit);
        m_runState = RunState::Stopped;
    });
    m_playModeButton.setOnClick([this]() {
        m_viewport.setMode(EditorViewportMode::Play);
        m_runState = RunState::Playing;
    });
    m_simulateModeButton.setOnClick([this]() {
        m_viewport.setMode(EditorViewportMode::Simulate);
        m_runState = RunState::Playing;
    });
    m_hudEditModeButton.setOnClick([this]() {
        m_viewport.setMode(EditorViewportMode::HudEdit);
        m_runState = RunState::Stopped;
    });
    m_spritePathInput.setOnBrowse([this]() {
        FileDialogOptions options;
        options.title = "Load Sprite";
        options.filters = {
            {"Image files", {"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tga"}},
            {"All files", {"*.*"}},
        };
        if (!m_spritePathInput.value().empty()) {
            const std::filesystem::path currentPath = m_spritePathInput.value();
            options.initialDirectory = currentPath.has_parent_path() ? currentPath.parent_path() : std::filesystem::current_path();
        }
        if (const auto selectedPath = FileDialog::openFile(options)) {
            m_spritePathInput.setValue(selectedPath->string());
            m_loadedSpritePath = selectedPath->string();
            m_worldTestSpriteLoaded = true;
            if (m_worldTestSpriteCount == 0) {
                m_worldTestSpriteCount = 1;
            }
            spdlog::info("Editor UI: Sprite test path set to '{}'.", m_loadedSpritePath);
        }
    });
    m_spritePathInput.setOnValueChanged([this](const std::string_view value) {
        m_loadedSpritePath = std::string(value);
        m_worldTestSpriteLoaded = !m_loadedSpritePath.empty();
    });
    m_addSpriteButton.setOnClick([this]() {
        m_worldTestSpriteLoaded = true;
        if (m_loadedSpritePath.empty()) {
            m_loadedSpritePath = m_spritePathInput.value();
        }
        ++m_worldTestSpriteCount;
    });
    m_addManySpritesButton.setOnClick([this]() {
        m_worldTestSpriteLoaded = true;
        if (m_loadedSpritePath.empty()) {
            m_loadedSpritePath = m_spritePathInput.value();
        }
        m_worldTestSpriteCount += 96;
    });
    m_addTilemapButton.setOnClick([this]() { m_worldTestTilemap = !m_worldTestTilemap; });
    m_animateSpriteButton.setOnClick([this]() { m_worldTestAnimated = !m_worldTestAnimated; });
    m_toggleParallaxButton.setOnClick([this]() { m_worldTestParallax = !m_worldTestParallax; });
    m_toggleParticlesButton.setOnClick([this]() { m_worldTestParticles = !m_worldTestParticles; });
    m_toggleDebugShapesButton.setOnClick([this]() { m_worldTestDebugShapes = !m_worldTestDebugShapes; });
    m_clearWorldTestButton.setOnClick([this]() {
        m_worldTestSpriteCount = 0;
        m_worldTestTilemap = false;
        m_worldTestAnimated = false;
        m_worldTestParallax = false;
        m_worldTestParticles = false;
        m_worldTestDebugShapes = false;
    });

    for (std::size_t index = 0; index < m_hierarchyRows.size(); ++index) {
        m_hierarchyRows[index].setStyleClass("hierarchy-row");
        m_hierarchyRows[index].setOnClick([this, index]() {
            m_selectedHierarchyRow = static_cast<int>(index);
            spdlog::info("Editor UI: Hierarchy row {} selected.", index);
        });
    }
}

// Builds the native editor shell and lets widgets handle their own state.
void EditorUI::render(Renderer2D& renderer2D, Renderer2DWorld& renderer2DWorld, TextRenderer& textRenderer, const glm::uvec2& viewportSize, const Input& input)
{
    m_context.beginFrame({
        input.mousePosition(),
        input.scrollDelta(),
        input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT),
        input.isKeyPressed(GLFW_KEY_LEFT_SHIFT) || input.isKeyPressed(GLFW_KEY_RIGHT_SHIFT),
        input.clipboardText(),
        [&input](const std::string_view text) { input.setClipboardText(text); },
        input.typedCharacters(),
        uiKeys(input),
    });

    const float width = static_cast<float>(std::max(viewportSize.x, 1U));
    const float height = static_cast<float>(std::max(viewportSize.y, 1U));
    layoutWidgets(width, height);
    m_clearColorPicker.registerPopupLayer(m_context);
    updatePanelSplitters(width, height);
    m_inspectorScroll.update(m_context);
    layoutWidgets(width, height);
    m_clearColorPicker.registerPopupLayer(m_context);
    updateWidgets(textRenderer);

    const glm::vec2 viewportEnd = m_viewportBounds.position + m_viewportBounds.size;
    renderer2D.drawQuad({0.0f, 0.0f}, {width, m_viewportBounds.position.y}, m_style.windowBackground);
    renderer2D.drawQuad({0.0f, viewportEnd.y}, {width, height - viewportEnd.y}, m_style.windowBackground);
    renderer2D.drawQuad({0.0f, m_viewportBounds.position.y}, {m_viewportBounds.position.x, m_viewportBounds.size.y}, m_style.windowBackground);
    renderer2D.drawQuad({viewportEnd.x, m_viewportBounds.position.y}, {width - viewportEnd.x, m_viewportBounds.size.y}, m_style.windowBackground);
    renderer2D.drawQuad({0.0f, 0.0f}, {width, m_toolbarHeight}, m_editorStyle.toolbarFill);

    if (m_hierarchyVisible) {
        drawPanel(renderer2D, m_hierarchyBounds);
    }
    if (m_inspectorVisible) {
        drawPanel(renderer2D, m_inspectorBounds);
    }
    if (m_consoleVisible) {
        drawPanel(renderer2D, m_consoleBounds);
    }

    m_viewport.updateInteraction(
        {m_viewportBounds.position, m_viewportBounds.size, m_viewportClearColor},
        input.mousePosition(),
        input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT));

    if (m_showGrid) {
        drawViewportGrid(renderer2D, m_viewportBounds);
    }

    const glm::vec4 viewportBorder = m_viewport.focused() ? m_editorStyle.viewportFocusedBorder : m_editorStyle.viewportBorder;
    renderer2D.drawRect(m_viewportBounds.position, m_viewportBounds.size, viewportBorder, 2.0f);

    renderWidgets(renderer2D, textRenderer);
    renderPanelSplitters(renderer2D);

    if (m_inspectorVisible) {
        m_inspectorScroll.pushClip(renderer2D);
        m_inspectorScroll.renderScrollbar(renderer2D, m_style);
        m_inspectorScroll.popClip(renderer2D);
    }

    submitWorldRendererTestContent(renderer2DWorld);
    renderLabels(textRenderer);
    renderWorldRendererLabels(textRenderer, renderer2DWorld);
    m_clearColorPicker.renderPopup(m_context, renderer2D, textRenderer, m_style);
    m_context.endFrame();
}

void EditorUI::updateEditorCamera(const float deltaTime, const Input& input)
{
    m_worldTestElapsed += deltaTime;
    const auto axis = [&input](const int positive, const int negative) {
        return static_cast<float>(input.isKeyPressed(positive)) - static_cast<float>(input.isKeyPressed(negative));
    };
    m_viewport.updateEditorCamera(deltaTime, {
        .moveRight = axis(GLFW_KEY_D, GLFW_KEY_A),
        .moveUp = axis(GLFW_KEY_E, GLFW_KEY_Q),
        .moveForward = axis(GLFW_KEY_W, GLFW_KEY_S),
    });
}

const EditorViewport& EditorUI::viewport() const
{
    return m_viewport;
}

// Computes responsive panel and widget bounds for the current framebuffer size.
void EditorUI::layoutWidgets(const float width, const float height)
{
    const float gap = m_style.gap;
    m_toolbarHeight = std::clamp(height * 0.07f, m_editorStyle.toolbarHeightMin, m_editorStyle.toolbarHeightMax);
    m_hierarchyVisible = !m_hierarchyCollapsed && width >= 760.0f;
    m_inspectorVisible = !m_inspectorCollapsed && width >= 620.0f;
    m_consoleVisible = !m_consoleCollapsed && height >= 460.0f;

    m_hierarchyWidth = std::clamp(m_hierarchyWidth, 150.0f, 320.0f);
    m_inspectorWidth = std::clamp(m_inspectorWidth, 190.0f, 380.0f);
    m_consoleHeight = std::clamp(m_consoleHeight, 96.0f, 260.0f);

    const float availableWorkWidth = std::max(width - gap * 2.0f, 1.0f);

    UIDockLayout shellLayout;
    shellLayout.setBounds({{gap, m_toolbarHeight + gap}, {availableWorkWidth, std::max(0.0f, height - m_toolbarHeight - gap * 2.0f)}});
    shellLayout.setGap(gap);
    if (m_consoleVisible) {
        shellLayout.add(m_consoleBounds, UIDock::Bottom, m_consoleHeight);
    }
    if (m_hierarchyVisible) {
        shellLayout.add(m_hierarchyBounds, UIDock::Left, std::min(m_hierarchyWidth, availableWorkWidth * 0.28f));
    }
    if (m_inspectorVisible) {
        shellLayout.add(m_inspectorBounds, UIDock::Right, std::min(m_inspectorWidth, availableWorkWidth * 0.34f));
    }
    shellLayout.add(m_viewportBounds, UIDock::Fill);
    shellLayout.layout();

    m_hierarchySplitterBounds = m_hierarchyVisible
        ? UIRect{{m_hierarchyBounds.position.x + m_hierarchyBounds.size.x, m_hierarchyBounds.position.y}, {gap, m_hierarchyBounds.size.y}}
        : UIRect{};
    m_inspectorSplitterBounds = m_inspectorVisible
        ? UIRect{{m_inspectorBounds.position.x - gap, m_inspectorBounds.position.y}, {gap, m_inspectorBounds.size.y}}
        : UIRect{};
    m_consoleSplitterBounds = m_consoleVisible
        ? UIRect{{m_consoleBounds.position.x, m_consoleBounds.position.y - gap}, {m_consoleBounds.size.x, gap}}
        : UIRect{};

    UILinearLayout toolbarLayout(UILayoutAxis::Horizontal);
    toolbarLayout.setBounds({{16.0f, 10.0f}, {std::max(0.0f, width - 32.0f), 28.0f}});
    toolbarLayout.setGap(12.0f);
    toolbarLayout.add(m_playButton, {76.0f, 28.0f});
    toolbarLayout.add(m_pauseButton, {76.0f, 28.0f});
    toolbarLayout.add(m_stopButton, {76.0f, 28.0f});
    toolbarLayout.add(m_toggleHierarchyButton, {28.0f, 28.0f});
    toolbarLayout.add(m_toggleInspectorButton, {28.0f, 28.0f});
    toolbarLayout.add(m_toggleConsoleButton, {28.0f, 28.0f});
    toolbarLayout.add(m_editModeButton, {52.0f, 28.0f});
    toolbarLayout.add(m_playModeButton, {52.0f, 28.0f});
    toolbarLayout.add(m_simulateModeButton, {76.0f, 28.0f});
    toolbarLayout.add(m_hudEditModeButton, {68.0f, 28.0f});
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
    const float scrollOffset = m_inspectorScroll.offset();
    inspectorLayout.add(m_gridCheckbox, UIAnchors::fixed({0.0f, 0.0f}, {0.0f, -scrollOffset}, {22.0f, 22.0f}));
    inspectorLayout.add(
    m_clearColorPicker,
    UIAnchors::fixed(
        {0.0f, 0.0f},
        {0.0f, 54.0f - scrollOffset},
        {30.0f, 30.0f}));
    inspectorLayout.add(m_exposureSlider, UIAnchors::horizontalStretch(148.0f - scrollOffset, 24.0f));
    inspectorLayout.add(m_lightIntensityInput, UIAnchors::horizontalStretch(214.0f - scrollOffset, 28.0f));
    const float coordinateGap = 6.0f;
    const float coordinateWidth = std::max((m_inspectorBounds.size.x - 36.0f - coordinateGap * 2.0f) / 3.0f, 0.0f);
    inspectorLayout.add(m_positionXInput, UIAnchors::fixed({0.0f, 0.0f}, {0.0f, 280.0f - scrollOffset}, {coordinateWidth, 28.0f}));
    inspectorLayout.add(m_positionYInput, UIAnchors::fixed({0.0f, 0.0f}, {coordinateWidth + coordinateGap, 280.0f - scrollOffset}, {coordinateWidth, 28.0f}));
    inspectorLayout.add(m_positionZInput, UIAnchors::fixed({0.0f, 0.0f}, {(coordinateWidth + coordinateGap) * 2.0f, 280.0f - scrollOffset}, {coordinateWidth, 28.0f}));
    inspectorLayout.add(m_objectNameInput, UIAnchors::horizontalStretch(368.0f - scrollOffset, 28.0f));
    inspectorLayout.add(m_spritePathInput, UIAnchors::horizontalStretch(492.0f - scrollOffset, 28.0f));
    const float worldButtonWidth = std::max((m_inspectorBounds.size.x - 36.0f - 8.0f) * 0.5f, 0.0f);
    inspectorLayout.add(m_addSpriteButton, UIAnchors::fixed({0.0f, 0.0f}, {0.0f, 534.0f - scrollOffset}, {worldButtonWidth, 28.0f}));
    inspectorLayout.add(m_addManySpritesButton, UIAnchors::fixed({0.0f, 0.0f}, {worldButtonWidth + 8.0f, 534.0f - scrollOffset}, {worldButtonWidth, 28.0f}));
    inspectorLayout.add(m_addTilemapButton, UIAnchors::fixed({0.0f, 0.0f}, {0.0f, 570.0f - scrollOffset}, {worldButtonWidth, 28.0f}));
    inspectorLayout.add(m_animateSpriteButton, UIAnchors::fixed({0.0f, 0.0f}, {worldButtonWidth + 8.0f, 570.0f - scrollOffset}, {worldButtonWidth, 28.0f}));
    inspectorLayout.add(m_toggleParallaxButton, UIAnchors::fixed({0.0f, 0.0f}, {0.0f, 606.0f - scrollOffset}, {worldButtonWidth, 28.0f}));
    inspectorLayout.add(m_toggleParticlesButton, UIAnchors::fixed({0.0f, 0.0f}, {worldButtonWidth + 8.0f, 606.0f - scrollOffset}, {worldButtonWidth, 28.0f}));
    inspectorLayout.add(m_toggleDebugShapesButton, UIAnchors::fixed({0.0f, 0.0f}, {0.0f, 642.0f - scrollOffset}, {worldButtonWidth, 28.0f}));
    inspectorLayout.add(m_clearWorldTestButton, UIAnchors::fixed({0.0f, 0.0f}, {worldButtonWidth + 8.0f, 642.0f - scrollOffset}, {worldButtonWidth, 28.0f}));
    inspectorLayout.layout();
    m_inspectorScroll.setBounds({
        {m_inspectorBounds.position.x + 12.0f, m_inspectorBounds.position.y + 36.0f},
        {std::max(m_inspectorBounds.size.x - 20.0f, 0.0f), std::max(m_inspectorBounds.size.y - 48.0f, 0.0f)},
    });
    m_inspectorScroll.setContentHeight(840.0f);

    m_gridCheckbox.setVisible(m_inspectorVisible);
    m_clearColorPicker.setVisible(m_inspectorVisible);
    m_exposureSlider.setVisible(m_inspectorVisible);
    m_lightIntensityInput.setVisible(m_inspectorVisible);
    m_positionXInput.setVisible(m_inspectorVisible);
    m_positionYInput.setVisible(m_inspectorVisible);
    m_positionZInput.setVisible(m_inspectorVisible);
    m_objectNameInput.setVisible(m_inspectorVisible);
    m_spritePathInput.setVisible(m_inspectorVisible);
    m_addSpriteButton.setVisible(m_inspectorVisible);
    m_addManySpritesButton.setVisible(m_inspectorVisible);
    m_addTilemapButton.setVisible(m_inspectorVisible);
    m_animateSpriteButton.setVisible(m_inspectorVisible);
    m_toggleParallaxButton.setVisible(m_inspectorVisible);
    m_toggleParticlesButton.setVisible(m_inspectorVisible);
    m_toggleDebugShapesButton.setVisible(m_inspectorVisible);
    m_clearWorldTestButton.setVisible(m_inspectorVisible);
    for (Button& row : m_hierarchyRows) {
        row.setVisible(m_hierarchyVisible);
    }
}

// Lets the editor shell resize docked panels without coupling splitters to renderer code.
void EditorUI::updatePanelSplitters(const float width, const float height)
{
    const glm::vec2 mousePosition = m_context.mousePosition();
    const glm::vec2 delta = mousePosition - m_previousMousePosition;
    bool changed = false;

    if (m_hierarchyVisible && m_context.interact("splitter.hierarchy", m_hierarchySplitterBounds.position, m_hierarchySplitterBounds.size).held) {
        m_hierarchyWidth = std::clamp(m_hierarchyWidth + delta.x, 150.0f, 320.0f);
        changed = true;
    }
    if (m_inspectorVisible && m_context.interact("splitter.inspector", m_inspectorSplitterBounds.position, m_inspectorSplitterBounds.size).held) {
        m_inspectorWidth = std::clamp(m_inspectorWidth - delta.x, 190.0f, 380.0f);
        changed = true;
    }
    if (m_consoleVisible && m_context.interact("splitter.console", m_consoleSplitterBounds.position, m_consoleSplitterBounds.size).held) {
        m_consoleHeight = std::clamp(m_consoleHeight - delta.y, 96.0f, 260.0f);
        changed = true;
    }

    m_previousMousePosition = mousePosition;
    if (changed) {
        layoutWidgets(width, height);
    }
}

// Updates every retained widget after layout has assigned its bounds.
void EditorUI::updateWidgets(TextRenderer& textRenderer)
{
    m_playButton.setSelected(m_runState == RunState::Playing);
    m_pauseButton.setSelected(m_runState == RunState::Paused);
    m_stopButton.setSelected(m_runState == RunState::Stopped);
    m_editModeButton.setSelected(m_viewport.mode() == EditorViewportMode::Edit);
    m_playModeButton.setSelected(m_viewport.mode() == EditorViewportMode::Play);
    m_simulateModeButton.setSelected(m_viewport.mode() == EditorViewportMode::Simulate);
    m_hudEditModeButton.setSelected(m_viewport.mode() == EditorViewportMode::HudEdit);
    m_gridCheckbox.setChecked(m_showGrid);
    m_exposureSlider.setValue(m_previewExposure);
    m_lightIntensityInput.setValue(m_lightIntensity);
    m_positionXInput.setValue(m_previewPosition.x);
    m_positionYInput.setValue(m_previewPosition.y);
    m_positionZInput.setValue(m_previewPosition.z);
    m_addTilemapButton.setSelected(m_worldTestTilemap);
    m_animateSpriteButton.setSelected(m_worldTestAnimated);
    m_toggleParallaxButton.setSelected(m_worldTestParallax);
    m_toggleParticlesButton.setSelected(m_worldTestParticles);
    m_toggleDebugShapesButton.setSelected(m_worldTestDebugShapes);

    m_playButton.update(m_context);
    m_pauseButton.update(m_context);
    m_stopButton.update(m_context);
    m_toggleHierarchyButton.update(m_context);
    m_toggleInspectorButton.update(m_context);
    m_toggleConsoleButton.update(m_context);
    m_editModeButton.update(m_context);
    m_playModeButton.update(m_context);
    m_simulateModeButton.update(m_context);
    m_hudEditModeButton.update(m_context);

    for (std::size_t index = 0; index < m_hierarchyRows.size(); ++index) {
        m_hierarchyRows[index].setSelected(static_cast<int>(index) == m_selectedHierarchyRow);
        m_hierarchyRows[index].update(m_context);
    }

    if (m_inspectorVisible) {
        m_inspectorScroll.pushClip(m_context);
        m_gridCheckbox.update(m_context);
        m_clearColorPicker.update(m_context);
        const UITextStyle& inputValueStyle = m_style.resolveText("input-value");
        m_exposureSlider.update(m_context, textRenderer, m_style, inputValueStyle.font, inputValueStyle.scale);
        m_lightIntensityInput.update(m_context, textRenderer, m_style, inputValueStyle.font, inputValueStyle.scale);
        m_positionXInput.update(m_context, textRenderer, m_style, inputValueStyle.font, inputValueStyle.scale);
        m_positionYInput.update(m_context, textRenderer, m_style, inputValueStyle.font, inputValueStyle.scale);
        m_positionZInput.update(m_context, textRenderer, m_style, inputValueStyle.font, inputValueStyle.scale);
        m_objectNameInput.update(m_context, textRenderer, m_style, inputValueStyle.font, inputValueStyle.scale);
        m_spritePathInput.update(m_context, textRenderer, m_style, inputValueStyle.font, inputValueStyle.scale);
        m_addSpriteButton.update(m_context);
        m_addManySpritesButton.update(m_context);
        m_addTilemapButton.update(m_context);
        m_animateSpriteButton.update(m_context);
        m_toggleParallaxButton.update(m_context);
        m_toggleParticlesButton.update(m_context);
        m_toggleDebugShapesButton.update(m_context);
        m_clearWorldTestButton.update(m_context);
        m_inspectorScroll.popClip(m_context);
        m_clearColorPicker.updatePopup(m_context, textRenderer, m_style);
    }
}

// Renders every retained widget and any editor-specific icon overlays.
void EditorUI::renderWidgets(Renderer2D& renderer2D, TextRenderer& textRenderer)
{
    UIFrame frame{m_context, renderer2D, textRenderer, m_style};
    m_playButton.render(renderer2D, m_style);
    drawToolbarIcon(renderer2D, m_playButton.position(), m_playButton.size(), ToolbarIcon::Play, m_playButton.selected());
    m_pauseButton.render(renderer2D, m_style);
    drawToolbarIcon(renderer2D, m_pauseButton.position(), m_pauseButton.size(), ToolbarIcon::Pause, m_pauseButton.selected());
    m_stopButton.render(renderer2D, m_style);
    drawToolbarIcon(renderer2D, m_stopButton.position(), m_stopButton.size(), ToolbarIcon::Stop, m_stopButton.selected());
    m_toggleHierarchyButton.render(renderer2D, m_style);
    m_toggleInspectorButton.render(renderer2D, m_style);
    m_toggleConsoleButton.render(renderer2D, m_style);
    m_editModeButton.render(renderer2D, m_style);
    m_playModeButton.render(renderer2D, m_style);
    m_simulateModeButton.render(renderer2D, m_style);
    m_hudEditModeButton.render(renderer2D, m_style);

    for (const auto& row : m_hierarchyRows) {
        row.render(renderer2D, m_style);
    }

    if (m_inspectorVisible) {
        m_inspectorScroll.pushClip(renderer2D);
        m_inspectorScroll.pushClip(textRenderer);
        m_gridCheckbox.render(renderer2D, m_style);
        m_clearColorPicker.render(renderer2D, m_style);
        m_exposureSlider.render(frame);
        m_lightIntensityInput.render(frame);
        m_positionXInput.render(frame);
        m_positionYInput.render(frame);
        m_positionZInput.render(frame);
        m_objectNameInput.render(frame);
        m_spritePathInput.render(frame);
        m_addSpriteButton.render(renderer2D, m_style);
        m_addManySpritesButton.render(renderer2D, m_style);
        m_addTilemapButton.render(renderer2D, m_style);
        m_animateSpriteButton.render(renderer2D, m_style);
        m_toggleParallaxButton.render(renderer2D, m_style);
        m_toggleParticlesButton.render(renderer2D, m_style);
        m_toggleDebugShapesButton.render(renderer2D, m_style);
        m_clearWorldTestButton.render(renderer2D, m_style);
        m_inspectorScroll.popClip(textRenderer);
        m_inspectorScroll.popClip(renderer2D);
    }
}

void EditorUI::renderPanelSplitters(Renderer2D& renderer2D)
{
    const glm::vec4 handle{0.30f, 0.36f, 0.46f, 1.0f};
    if (m_hierarchyVisible) {
        renderer2D.drawQuad({m_hierarchySplitterBounds.position.x + 3.0f, m_hierarchySplitterBounds.position.y}, {2.0f, m_hierarchySplitterBounds.size.y}, handle);
    }
    if (m_inspectorVisible) {
        renderer2D.drawQuad({m_inspectorSplitterBounds.position.x + 3.0f, m_inspectorSplitterBounds.position.y}, {2.0f, m_inspectorSplitterBounds.size.y}, handle);
    }
    if (m_consoleVisible) {
        renderer2D.drawQuad({m_consoleSplitterBounds.position.x, m_consoleSplitterBounds.position.y + 3.0f}, {m_consoleSplitterBounds.size.x, 2.0f}, handle);
    }
}

// Draws the first real editor labels through the cached font atlas.
void EditorUI::renderLabels(TextRenderer& textRenderer)
{
    if (m_hierarchyVisible) {
        drawStyledText(textRenderer, "Hierarchy", {m_hierarchyBounds.position + glm::vec2{16.0f, 8.0f}, {m_hierarchyBounds.size.x - 32.0f, 18.0f}}, "heading");
    }
    if (m_inspectorVisible) {
        drawStyledText(textRenderer, "Inspector", {m_inspectorBounds.position + glm::vec2{18.0f, 8.0f}, {m_inspectorBounds.size.x - 36.0f, 18.0f}}, "heading");
    }
    if (m_consoleVisible) {
        drawStyledText(textRenderer, "Console", {m_consoleBounds.position + glm::vec2{16.0f, 8.0f}, {m_consoleBounds.size.x - 32.0f, 18.0f}}, "heading");
    }
    drawStyledText(textRenderer, "Viewport", {m_viewportBounds.position + glm::vec2{12.0f, 8.0f}, {m_viewportBounds.size.x - 24.0f, 18.0f}}, "muted", "viewport-title");
    drawStyledText(textRenderer, "H", {m_toggleHierarchyButton.position(), m_toggleHierarchyButton.size()}, "toolbar-toggle");
    drawStyledText(textRenderer, "I", {m_toggleInspectorButton.position(), m_toggleInspectorButton.size()}, "toolbar-toggle");
    drawStyledText(textRenderer, "C", {m_toggleConsoleButton.position(), m_toggleConsoleButton.size()}, "toolbar-toggle");
    drawStyledText(textRenderer, "Edit", {m_editModeButton.position(), m_editModeButton.size()}, "toolbar-toggle");
    drawStyledText(textRenderer, "Play", {m_playModeButton.position(), m_playModeButton.size()}, "toolbar-toggle");
    drawStyledText(textRenderer, "Simulate", {m_simulateModeButton.position(), m_simulateModeButton.size()}, "toolbar-toggle");
    drawStyledText(textRenderer, "HudEdit", {m_hudEditModeButton.position(), m_hudEditModeButton.size()}, "toolbar-toggle");
    drawStyledText(
        textRenderer,
        editorViewportModeName(m_viewport.mode()),
        {m_viewportBounds.position + glm::vec2{12.0f, 28.0f}, {m_viewportBounds.size.x - 24.0f, 18.0f}},
        "muted");

    constexpr std::array<std::string_view, 5> hierarchyNames = {
        "Main Camera",
        "Directional Light",
        "Environment",
        "Player",
        "UI Canvas",
    };

    for (std::size_t index = 0; m_hierarchyVisible && index < m_hierarchyRows.size(); ++index) {
        drawStyledText(
            textRenderer,
            hierarchyNames[index],
            {m_hierarchyRows[index].position(), m_hierarchyRows[index].size()},
            "hierarchy-row");
    }

    if (m_inspectorVisible) {
        m_inspectorScroll.pushClip(textRenderer);
        drawStyledText(
            textRenderer,
            "Grid",
            {{m_gridCheckbox.position().x + 32.0f, m_gridCheckbox.position().y}, {m_inspectorBounds.size.x - 68.0f, m_gridCheckbox.size().y}},
            "control-label");
        drawStyledText(
            textRenderer,
            "Viewport Clear",
            {{m_clearColorPicker.position().x, m_clearColorPicker.position().y - 22.0f}, {m_clearColorPicker.size().x, 18.0f}},
            "control-label");
        drawStyledText(
            textRenderer,
            "Exposure",
            {{m_exposureSlider.position().x, m_exposureSlider.position().y - 22.0f}, {m_exposureSlider.size().x, 18.0f}},
            "control-label");
        drawStyledText(
            textRenderer,
            "Light Intensity",
            {{m_lightIntensityInput.position().x, m_lightIntensityInput.position().y - 22.0f}, {m_lightIntensityInput.size().x, 18.0f}},
            "control-label");
        drawStyledText(
            textRenderer,
            "Position",
            {{m_positionXInput.position().x, m_positionXInput.position().y - 22.0f}, {m_positionZInput.position().x + m_positionZInput.size().x - m_positionXInput.position().x, 18.0f}},
            "control-label");
        const auto drawCoordinateInput = [this, &textRenderer](const char axis, const NumberInput& input) {
            if (!input.editing()) {
                drawStyledText(
                textRenderer,
                std::string_view{&axis, 1},
                {{input.position().x + 6.0f, input.position().y}, {12.0f, input.size().y}},
                "coordinate-axis");
            }
        };
        drawCoordinateInput('X', m_positionXInput);
        drawCoordinateInput('Y', m_positionYInput);
        drawCoordinateInput('Z', m_positionZInput);
        std::ostringstream positionText;
        positionText << '{' << m_positionXInput.formattedValue() << ", "
                     << m_positionYInput.formattedValue() << ", "
                     << m_positionZInput.formattedValue() << '}';
        drawStyledText(
            textRenderer,
            positionText.str(),
            {{m_positionXInput.position().x, m_positionXInput.position().y + 32.0f}, {m_positionZInput.position().x + m_positionZInput.size().x - m_positionXInput.position().x, 18.0f}},
            "coordinate-summary");
        drawStyledText(
            textRenderer,
            "Object Name",
            {{m_objectNameInput.position().x, m_objectNameInput.position().y - 22.0f}, {m_objectNameInput.size().x, 18.0f}},
            "control-label");
        drawStyledText(
            textRenderer,
            "2D World Sprite Test",
            {{m_spritePathInput.position().x, m_spritePathInput.position().y - 40.0f}, {m_spritePathInput.size().x, 18.0f}},
            "heading");
        drawStyledText(
            textRenderer,
            "Sprite Path",
            {{m_spritePathInput.position().x, m_spritePathInput.position().y - 18.0f}, {m_spritePathInput.size().x, 16.0f}},
            "control-label");
        drawStyledText(textRenderer, "Add", {m_addSpriteButton.position(), m_addSpriteButton.size()}, "toolbar-toggle");
        drawStyledText(textRenderer, "Add Many", {m_addManySpritesButton.position(), m_addManySpritesButton.size()}, "toolbar-toggle");
        drawStyledText(textRenderer, "Tilemap", {m_addTilemapButton.position(), m_addTilemapButton.size()}, "toolbar-toggle");
        drawStyledText(textRenderer, "Animate", {m_animateSpriteButton.position(), m_animateSpriteButton.size()}, "toolbar-toggle");
        drawStyledText(textRenderer, "Parallax", {m_toggleParallaxButton.position(), m_toggleParallaxButton.size()}, "toolbar-toggle");
        drawStyledText(textRenderer, "Particles", {m_toggleParticlesButton.position(), m_toggleParticlesButton.size()}, "toolbar-toggle");
        drawStyledText(textRenderer, "Debug", {m_toggleDebugShapesButton.position(), m_toggleDebugShapesButton.size()}, "toolbar-toggle");
        drawStyledText(textRenderer, "Clear Sprite Test Scene", {m_clearWorldTestButton.position(), m_clearWorldTestButton.size()}, "toolbar-toggle");
        m_inspectorScroll.popClip(textRenderer);
    }
    if (m_consoleVisible) {
        drawStyledText(
            textRenderer,
            "Editor initialized. Text atlas rendering active.",
            {m_consoleBounds.position + glm::vec2{16.0f, 34.0f}, {m_consoleBounds.size.x - 32.0f, 18.0f}},
            "muted");
    }
}

void EditorUI::submitWorldRendererTestContent(Renderer2DWorld& renderer2DWorld)
{
    const auto textureFromPath = [this]() {
        const std::string& path = m_loadedSpritePath.empty() ? m_spritePathInput.value() : m_loadedSpritePath;
        const WorldTextureId hash = static_cast<WorldTextureId>(std::hash<std::string>{}(path));
        return hash == 0 ? WorldTextureId{1} : hash;
    };

    renderer2DWorld.begin({
        .position = {0.0f, 0.0f},
        .viewportSize = glm::max(m_viewportBounds.size, glm::vec2{1.0f, 1.0f}),
        .zoom = 1.0f,
    });

    const WorldTextureId spriteTexture = textureFromPath();
    const std::size_t spriteCount = std::min(m_worldTestSpriteCount, std::size_t{512});
    for (std::size_t index = 0; index < spriteCount; ++index) {
        const float column = static_cast<float>(index % 16U);
        const float row = static_cast<float>(index / 16U);
        const glm::vec4 tint{
            0.25f + 0.12f * static_cast<float>(index % 5U),
            0.58f + 0.08f * static_cast<float>(index % 3U),
            0.90f - 0.08f * static_cast<float>(index % 4U),
            1.0f,
        };
        renderer2DWorld.drawSprite(
            spriteTexture,
            {
                .position = {-220.0f + column * 28.0f, 120.0f - row * 28.0f, static_cast<float>(index % 6U)},
                .size = {22.0f, 22.0f},
                .rotationRadians = (m_worldTestAnimated ? m_worldTestElapsed * 0.6f : 0.0f) + static_cast<float>(index % 4U) * 0.08f,
                .layer = static_cast<int>(index % 3U),
            },
            {},
            tint,
            static_cast<int>(index));
    }

    if (m_worldTestTilemap) {
        WorldTilemap tilemap;
        tilemap.columns = 8;
        tilemap.rows = 5;
        tilemap.tileSize = {24.0f, 24.0f};
        tilemap.tiles.reserve(static_cast<std::size_t>(tilemap.columns) * tilemap.rows);
        for (std::uint32_t index = 0; index < tilemap.columns * tilemap.rows; ++index) {
            const bool gap = index % 7U == 0U;
            tilemap.tiles.push_back({
                .texture = spriteTexture + 10U,
                .tint = gap ? glm::vec4{0.0f, 0.0f, 0.0f, 0.0f} : glm::vec4{0.26f, 0.72f, 0.42f, 1.0f},
                .entityId = static_cast<int>(1000U + index),
                .solid = !gap,
            });
        }
        renderer2DWorld.drawTilemap(tilemap, {.position = {-260.0f, -80.0f, -3.0f}, .origin = {0.0f, 0.0f}, .layer = -2});
    }

    if (m_worldTestAnimated) {
        renderer2DWorld.drawAnimatedSprite(
            {.texture = spriteTexture + 20U, .frameGrid = {4U, 2U}, .frameCount = 8U, .framesPerSecond = 8.0f},
            m_worldTestElapsed,
            {.position = {0.0f, 0.0f, 8.0f}, .size = {54.0f, 54.0f}, .rotationRadians = std::sin(m_worldTestElapsed) * 0.35f, .layer = 8},
            {1.0f, 0.78f, 0.28f, 1.0f},
            2000);
    }

    if (m_worldTestParallax) {
        renderer2DWorld.drawParallaxSprite(
            spriteTexture + 30U,
            {.position = {-150.0f + std::sin(m_worldTestElapsed * 0.7f) * 40.0f, -150.0f, -20.0f}, .size = {180.0f, 42.0f}, .layer = -10},
            {0.35f, 0.2f},
            {},
            {0.24f, 0.34f, 0.70f, 0.85f},
            3000);
    }

    if (m_worldTestParticles) {
        std::array<WorldParticle, 24> particles{};
        for (std::size_t index = 0; index < particles.size(); ++index) {
            const float angle = m_worldTestElapsed * 1.8f + static_cast<float>(index) * 0.45f;
            const float radius = 42.0f + static_cast<float>(index % 5U) * 8.0f;
            particles[index] = {
                .texture = spriteTexture + 40U,
                .transform = {
                    .position = {std::cos(angle) * radius + 160.0f, std::sin(angle) * radius - 10.0f, 5.0f},
                    .size = {8.0f, 8.0f},
                    .layer = 6,
                },
                .tint = {1.0f, 0.38f + 0.02f * static_cast<float>(index), 0.24f, 0.85f},
                .entityId = static_cast<int>(4000U + index),
            };
        }
        renderer2DWorld.drawParticles(particles);
    }

    if (m_worldTestDebugShapes) {
        renderer2DWorld.drawDebugRect({-280.0f, -140.0f}, {220.0f, 120.0f}, {0.2f, 0.85f, 1.0f, 1.0f}, 2.0f);
        renderer2DWorld.drawDebugCircle({110.0f, 96.0f}, 42.0f, {1.0f, 0.95f, 0.25f, 1.0f}, 2.0f, 28U);
        renderer2DWorld.drawDebugLine({-32.0f, -32.0f, 0.0f}, {84.0f, 46.0f, 0.0f}, {1.0f, 0.22f, 0.35f, 1.0f}, 2.0f);
    }

    renderer2DWorld.end();
}

void EditorUI::renderWorldRendererLabels(TextRenderer& textRenderer, const Renderer2DWorld& renderer2DWorld)
{
    const Renderer2DWorldStats stats = renderer2DWorld.stats();

    std::ostringstream viewportText;
    viewportText << "World2D target: " << stats.quadCount << " quads, "
                 << stats.drawBatchCount << " batches, "
                 << stats.debugLineCount << " debug";
    drawStyledText(
        textRenderer,
        viewportText.str(),
        {m_viewportBounds.position + glm::vec2{12.0f, 48.0f}, {m_viewportBounds.size.x - 24.0f, 18.0f}},
        "muted");

    if (!m_inspectorVisible) {
        return;
    }

    m_inspectorScroll.pushClip(textRenderer);
    std::ostringstream status;
    status << "Loaded: " << (m_worldTestSpriteLoaded ? "yes" : "no")
           << " | Sprites: " << m_worldTestSpriteCount
           << " | Quads: " << stats.quadCount
           << " | Batches: " << stats.drawBatchCount;
    drawStyledText(
        textRenderer,
        status.str(),
        {{m_clearWorldTestButton.position().x, m_clearWorldTestButton.position().y + 38.0f}, {m_clearWorldTestButton.size().x, 18.0f}},
        "muted");
    m_inspectorScroll.popClip(textRenderer);
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
        renderer2D.drawQuad({x, bounds.position.y}, {1.0f, bounds.size.y}, m_editorStyle.viewportGrid);
    }

    for (float y = bounds.position.y + spacing; y < bounds.position.y + bounds.size.y; y += spacing) {
        renderer2D.drawQuad({bounds.position.x, y}, {bounds.size.x, 1.0f}, m_editorStyle.viewportGrid);
    }
}

} // namespace Engine
