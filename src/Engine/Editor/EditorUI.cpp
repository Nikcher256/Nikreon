#include "Engine/Editor/EditorUI.hpp"

#include "Engine/Core/FileDialog.hpp"
#include "Engine/Core/Input.hpp"
#include "Engine/Renderer/Renderer2D.hpp"
#include "Engine/Renderer/TextRenderer.hpp"
#include "Engine/UI/UIStyleParser.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

namespace Engine {

#ifndef NIKREON_ASSET_DIR
#define NIKREON_ASSET_DIR "assets"
#endif

namespace {

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

std::array<std::string_view, 5> hierarchyNames()
{
    return {
        "Main Camera",
        "Directional Light",
        "Environment",
        "Player",
        "UI Canvas",
    };
}

void drawQuadIfPositive(Renderer2D& renderer2D, const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
{
    if (size.x <= 0.0f || size.y <= 0.0f) {
        return;
    }
    renderer2D.drawQuad(position, size, color);
}

void drawEditorBackgroundOutsideViewport(Renderer2D& renderer2D, const glm::vec2& canvasSize, const UIRect& viewportBounds, const glm::vec4& color)
{
    const float left = std::clamp(viewportBounds.position.x, 0.0f, canvasSize.x);
    const float top = std::clamp(viewportBounds.position.y, 0.0f, canvasSize.y);
    const float right = std::clamp(viewportBounds.position.x + viewportBounds.size.x, 0.0f, canvasSize.x);
    const float bottom = std::clamp(viewportBounds.position.y + viewportBounds.size.y, 0.0f, canvasSize.y);

    drawQuadIfPositive(renderer2D, {0.0f, 0.0f}, {canvasSize.x, top}, color);
    drawQuadIfPositive(renderer2D, {0.0f, bottom}, {canvasSize.x, canvasSize.y - bottom}, color);
    drawQuadIfPositive(renderer2D, {0.0f, top}, {left, bottom - top}, color);
    drawQuadIfPositive(renderer2D, {right, top}, {canvasSize.x - right, bottom - top}, color);
}

} // namespace

EditorUI::EditorUI(EditorViewport& viewport, EditorWorldDebugController& worldDebug)
    : m_viewport(viewport)
    , m_worldDebug(worldDebug)
{
    std::string styleError;
    if (!UIStyleParser::loadFile(NIKREON_ASSET_DIR "/styles/editor.ui.css", m_style, styleError)) {
        spdlog::warn("Editor UI stylesheet was not loaded: {}", styleError);
    }
}

void EditorUI::render(Renderer2D& renderer2D, TextRenderer& textRenderer, const glm::uvec2& viewportSize, const Input& input)
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
    m_renderSize = {width, height};

    m_ui.begin(m_context, renderer2D, textRenderer, m_style, {{0.0f, 0.0f}, {width, height}});
    declareUI(width, height);
    m_ui.layout();
    syncBuilderBounds();
    if (updatePanelSplitters()) {
        declareUI(width, height);
        m_ui.layout();
        syncBuilderBounds();
    }
    m_ui.update();

    drawEditorBackgroundOutsideViewport(renderer2D, {width, height}, m_viewportBounds, m_style.windowBackground);

    const bool viewportInputBlocked = m_context.baseInputBlocked();
    const glm::vec2 viewportMousePosition = viewportInputBlocked
        ? glm::vec2{-1'000'000.0f, -1'000'000.0f}
        : input.mousePosition();
    m_viewport.updateInteraction(
        {m_viewportBounds.position, m_viewportBounds.size, m_viewportClearColor},
        viewportMousePosition,
        !viewportInputBlocked && input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT));

    if (m_showGrid) {
        drawViewportGrid(renderer2D, m_viewportBounds);
    }

    const glm::vec4 viewportBorder = m_viewport.focused() ? m_editorStyle.viewportFocusedBorder : m_editorStyle.viewportBorder;
    renderer2D.drawRect(m_viewportBounds.position, m_viewportBounds.size, viewportBorder, 2.0f);

    renderPanelSplitters(renderer2D);
    m_ui.render();

    m_context.endFrame();
    m_ui.end();
}

void EditorUI::update(const float deltaTime)
{
    m_worldDebug.update(deltaTime);
}

void EditorUI::declareUI(const float width, const float height)
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
    const float hierarchyWidth = std::min(m_hierarchyWidth, availableWorkWidth * 0.28f);
    const float inspectorWidth = std::min(m_inspectorWidth, availableWorkWidth * 0.34f);

    m_ui.panel("toolbar")
        .dock(UIDock::Top)
        .height(m_toolbarHeight)
        .horizontal()
        .padding({16.0f, 10.0f, 16.0f, 10.0f})
        .gap(12.0f)
        .styleClass("toolbar-panel");

    m_ui.button("toolbar.play")
        .parent("toolbar")
        .styleClass("toolbar")
        .text("Play")
        .textStyle("toolbar-toggle")
        .width(76.0f)
        .selected(m_runState == RunState::Playing)
        .onClick([this]() {
            m_runState = RunState::Playing;
            m_viewport.setMode(EditorViewportMode::Play);
            spdlog::info("Editor UI: Play button clicked.");
        });

    m_ui.button("toolbar.pause")
        .parent("toolbar")
        .styleClass("toolbar")
        .text("Pause")
        .textStyle("toolbar-toggle")
        .width(76.0f)
        .selected(m_runState == RunState::Paused)
        .onClick([this]() {
            m_runState = RunState::Paused;
            spdlog::info("Editor UI: Pause button clicked.");
        });

    m_ui.button("toolbar.stop")
        .parent("toolbar")
        .styleClass("toolbar")
        .text("Stop")
        .textStyle("toolbar-toggle")
        .width(76.0f)
        .selected(m_runState == RunState::Stopped)
        .onClick([this]() {
            m_runState = RunState::Stopped;
            m_viewport.setMode(EditorViewportMode::Edit);
            spdlog::info("Editor UI: Stop button clicked.");
        });

    m_ui.button("toolbar.toggleHierarchy")
        .parent("toolbar")
        .styleClass("toolbar-toggle")
        .text("H")
        .textStyle("toolbar-toggle")
        .tooltip("Toggle hierarchy")
        .width(28.0f)
        .onClick([this]() { m_hierarchyCollapsed = !m_hierarchyCollapsed; });

    m_ui.button("toolbar.toggleInspector")
        .parent("toolbar")
        .styleClass("toolbar-toggle")
        .text("I")
        .textStyle("toolbar-toggle")
        .tooltip("Toggle inspector")
        .width(28.0f)
        .onClick([this]() { m_inspectorCollapsed = !m_inspectorCollapsed; });

    m_ui.button("toolbar.toggleConsole")
        .parent("toolbar")
        .styleClass("toolbar-toggle")
        .text("C")
        .textStyle("toolbar-toggle")
        .tooltip("Toggle console")
        .width(28.0f)
        .onClick([this]() { m_consoleCollapsed = !m_consoleCollapsed; });

    m_ui.dropdown("toolbar.viewportMode")
        .parent("toolbar")
        .styleClass("toolbar")
        .textStyle("toolbar-toggle")
        .width(118.0f)
        .items({"Edit", "Play", "Simulate", "HudEdit"})
        .selectedIndex(static_cast<std::size_t>(m_viewport.mode()))
        .onSelectionChanged([this](const std::size_t index, std::string_view) { setViewportModeFromIndex(index); });

    m_ui.dropdown("toolbar.cameraMode")
        .parent("toolbar")
        .styleClass("toolbar")
        .textStyle("toolbar-toggle")
        .width(146.0f)
        .items({"2D Camera", "3D Perspective"})
        .selectedIndex(static_cast<std::size_t>(m_viewport.cameraMode()))
        .onSelectionChanged([this](const std::size_t index, std::string_view) {
            m_viewport.setCameraMode(index == 1 ? EditorCameraMode::Perspective3D : EditorCameraMode::Orthographic2D);
        });

    m_ui.panel("console")
        .dock(UIDock::Bottom)
        .height(m_consoleHeight)
        .visible(m_consoleVisible)
        .vertical()
        .padding({16.0f, 8.0f, 16.0f, 16.0f})
        .gap(8.0f);

    m_ui.label("console.title")
        .parent("console")
        .text("Console")
        .textStyle("heading")
        .height(18.0f);

    m_ui.label("console.message")
        .parent("console")
        .text("Editor initialized. Text atlas rendering active.")
        .textStyle("muted")
        .height(18.0f);

    m_ui.panel("hierarchy")
        .dock(UIDock::Left)
        .width(hierarchyWidth)
        .visible(m_hierarchyVisible)
        .vertical()
        .padding({16.0f, 8.0f, 16.0f, 20.0f})
        .gap(12.0f);

    m_ui.label("hierarchy.title")
        .parent("hierarchy")
        .text("Hierarchy")
        .textStyle("heading")
        .height(18.0f);

    const auto names = hierarchyNames();
    for (std::size_t index = 0; index < names.size(); ++index) {
        const std::string id = "hierarchy.row." + std::to_string(index);
        m_ui.button(id)
            .parent("hierarchy")
            .styleClass("hierarchy-row")
            .text(names[index])
            .textStyle("hierarchy-row")
            .height(22.0f)
            .selected(static_cast<int>(index) == m_selectedHierarchyRow)
            .onClick([this, index]() {
                m_selectedHierarchyRow = static_cast<int>(index);
                spdlog::info("Editor UI: Hierarchy row {} selected.", index);
            });
    }

    m_ui.panel("inspector")
        .dock(UIDock::Right)
        .width(inspectorWidth)
        .visible(m_inspectorVisible)
        .vertical()
        .scrollable(true)
        .padding({18.0f, 8.0f, 18.0f, 18.0f})
        .gap(14.0f);

    m_ui.label("inspector.title")
        .parent("inspector")
        .text("Inspector")
        .textStyle("heading")
        .height(18.0f);

    m_ui.checkbox("inspector.showGrid")
        .parent("inspector")
        .label("Grid")
        .labelStyle("control-label")
        .labelPlacement(UILabelPlacement::Right)
        .checked(m_showGrid)
        .onCheckedChanged([this](const bool enabled) {
            m_showGrid = enabled;
            spdlog::info("Editor UI: Viewport grid {}.", m_showGrid ? "enabled" : "disabled");
        });

    m_ui.colorPicker("inspector.clearColor")
        .parent("inspector")
        .label("Viewport Clear")
        .labelStyle("control-label")
        .color(m_viewportClearColor)
        .onColorChanged([this](const glm::vec4& color) {
            m_viewportClearColor = color;
        });

    m_ui.sliderFloat("inspector.exposure")
        .parent("inspector")
        .styleClass("inspector")
        .label("Exposure")
        .labelStyle("control-label")
        .value(m_previewExposure)
        .range(0.0f, 1.0f)
        .precision(2)
        .onChanged([this](const float value) {
            m_previewExposure = value;
            if (std::abs(m_previewExposure - m_lastLoggedExposure) >= 0.05f) {
                m_lastLoggedExposure = m_previewExposure;
                spdlog::info("Editor UI: Preview brightness set to {:.2f}.", m_previewExposure);
            }
        });

    m_ui.numberFloat("inspector.lightIntensity")
        .parent("inspector")
        .styleClass("inspector")
        .label("Light Intensity")
        .labelStyle("control-label")
        .value(m_lightIntensity)
        .range(0.0f, 100.0f)
        .precision(2)
        .sensitivity(0.05f)
        .onChanged([this](const float value) { m_lightIntensity = value; });

    m_ui.label("inspector.position-label")
        .parent("inspector")
        .text("Position")
        .textStyle("control-label")
        .height(18.0f);

    m_ui.panel("inspector.position-grid")
        .parent("inspector")
        .drawBackground(false)
        .grid(3)
        .height(28.0f)
        .padding(UIEdgeInsets::all(0.0f))
        .gap(6.0f);

    m_ui.numberFloat("inspector.position.x")
        .parent("inspector.position-grid")
        .styleClass("plain")
        .value(m_previewPosition.x)
        .range(-100.0f, 100.0f)
        .precision(2)
        .sensitivity(0.1f)
        .onChanged([this](const float value) { m_previewPosition.x = value; });

    m_ui.numberFloat("inspector.position.y")
        .parent("inspector.position-grid")
        .styleClass("plain")
        .value(m_previewPosition.y)
        .range(-100.0f, 100.0f)
        .precision(2)
        .sensitivity(0.1f)
        .onChanged([this](const float value) { m_previewPosition.y = value; });

    m_ui.numberFloat("inspector.position.z")
        .parent("inspector.position-grid")
        .styleClass("plain")
        .value(m_previewPosition.z)
        .range(-100.0f, 100.0f)
        .precision(2)
        .sensitivity(0.1f)
        .onChanged([this](const float value) { m_previewPosition.z = value; });

    std::ostringstream positionText;
    positionText << std::fixed << std::setprecision(2)
                 << '{' << m_previewPosition.x << ", "
                 << m_previewPosition.y << ", "
                 << m_previewPosition.z << '}';
    m_ui.label("inspector.position-summary")
        .parent("inspector")
        .text(positionText.str())
        .textStyle("coordinate-summary")
        .height(18.0f);

    m_ui.textInput("inspector.objectName")
        .parent("inspector")
        .styleClass("inspector")
        .label("Object Name")
        .labelStyle("control-label")
        .value(m_objectName)
        .placeholder("Object name")
        .onTextChanged([this](const std::string_view value) { m_objectName = std::string(value); });

    m_ui.label("world2d.title")
        .parent("inspector")
        .text("2D World Sprite Test")
        .textStyle("heading")
        .height(18.0f);

    m_ui.filePathInput("world2d.spritePath")
        .parent("inspector")
        .styleClass("inspector")
        .label("Sprite Path")
        .labelStyle("control-label")
        .value(m_spritePath)
        .buttonLabel("Browse")
        .browseButtonWidth(78.0f)
        .onBrowse([this]() { openSpriteFileDialog(); })
        .onTextChanged([this](const std::string_view value) {
            m_spritePath = std::string(value);
            m_worldDebug.setSpritePath(m_spritePath);
        });

    m_ui.panel("world2d.actions")
        .parent("inspector")
        .drawBackground(false)
        .grid(2)
        .height(172.0f)
        .padding(UIEdgeInsets::all(0.0f))
        .gap(8.0f);

    m_ui.button("world2d.addSprite")
        .parent("world2d.actions")
        .styleClass("toolbar")
        .text("Add")
        .textStyle("toolbar-toggle")
        .height(28.0f)
        .onClick([this]() {
            m_worldDebug.setSpritePath(m_spritePath);
            m_worldDebug.addSprite();
        });

    m_ui.button("world2d.addManySprites")
        .parent("world2d.actions")
        .styleClass("toolbar")
        .text("Add Many")
        .textStyle("toolbar-toggle")
        .height(28.0f)
        .onClick([this]() {
            m_worldDebug.setSpritePath(m_spritePath);
            m_worldDebug.addManySprites();
        });

    m_ui.button("world2d.tilemap")
        .parent("world2d.actions")
        .styleClass("toolbar")
        .text("Tilemap")
        .textStyle("toolbar-toggle")
        .height(28.0f)
        .selected(m_worldDebug.tilemapEnabled())
        .onClick([this]() { m_worldDebug.toggleTilemap(); });

    m_ui.button("world2d.animate")
        .parent("world2d.actions")
        .styleClass("toolbar")
        .text("Animate")
        .textStyle("toolbar-toggle")
        .height(28.0f)
        .selected(m_worldDebug.animatedSpriteEnabled())
        .onClick([this]() { m_worldDebug.toggleAnimatedSprite(); });

    m_ui.button("world2d.parallax")
        .parent("world2d.actions")
        .styleClass("toolbar")
        .text("Parallax")
        .textStyle("toolbar-toggle")
        .height(28.0f)
        .selected(m_worldDebug.parallaxEnabled())
        .onClick([this]() { m_worldDebug.toggleParallax(); });

    m_ui.button("world2d.particles")
        .parent("world2d.actions")
        .styleClass("toolbar")
        .text("Particles")
        .textStyle("toolbar-toggle")
        .height(28.0f)
        .selected(m_worldDebug.particlesEnabled())
        .onClick([this]() { m_worldDebug.toggleParticles(); });

    m_ui.button("world2d.debugShapes")
        .parent("world2d.actions")
        .styleClass("toolbar")
        .text("Debug")
        .textStyle("toolbar-toggle")
        .height(28.0f)
        .selected(m_worldDebug.debugShapesEnabled())
        .onClick([this]() { m_worldDebug.toggleDebugShapes(); });

    m_ui.button("world2d.blendModes")
        .parent("world2d.actions")
        .styleClass("toolbar")
        .text("Blend")
        .textStyle("toolbar-toggle")
        .height(28.0f)
        .selected(m_worldDebug.blendModeTestEnabled())
        .tooltip("Blend Mode Test")
        .onClick([this]() { m_worldDebug.toggleBlendModeTest(); });

    m_ui.button("world2d.clear")
        .parent("world2d.actions")
        .styleClass("toolbar")
        .text("Clear Sprite Test Scene")
        .textStyle("toolbar-toggle")
        .height(28.0f)
        .tooltip("Clear Sprite Test Scene")
        .onClick([this]() { m_worldDebug.clear(); });

    m_ui.panel("viewport")
        .dock(UIDock::Fill)
        .drawBackground(false)
        .vertical()
        .padding({12.0f, 8.0f, 12.0f, 0.0f})
        .gap(4.0f);

    m_ui.label("viewport-title")
        .parent("viewport")
        .text("Viewport")
        .textStyle("muted")
        .height(18.0f);

    m_ui.label("viewport.mode")
        .parent("viewport")
        .text(editorViewportModeName(m_viewport.mode()))
        .textStyle("muted")
        .height(18.0f);
}

void EditorUI::syncBuilderBounds()
{
    m_hierarchyBounds = m_ui.bounds("hierarchy");
    m_inspectorBounds = m_ui.bounds("inspector");
    m_consoleBounds = m_ui.bounds("console");
    m_viewportBounds = m_ui.bounds("viewport");

    const float gap = m_style.gap;
    m_hierarchySplitterBounds = m_hierarchyVisible
        ? UIRect{{m_hierarchyBounds.position.x + m_hierarchyBounds.size.x, m_hierarchyBounds.position.y}, {gap, m_hierarchyBounds.size.y}}
        : UIRect{};
    m_inspectorSplitterBounds = m_inspectorVisible
        ? UIRect{{m_inspectorBounds.position.x - gap, m_inspectorBounds.position.y}, {gap, m_inspectorBounds.size.y}}
        : UIRect{};
    m_consoleSplitterBounds = m_consoleVisible
        ? UIRect{{m_consoleBounds.position.x, m_consoleBounds.position.y - gap}, {m_consoleBounds.size.x, gap}}
        : UIRect{};
}

bool EditorUI::updatePanelSplitters()
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
    return changed;
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

void EditorUI::setViewportModeFromIndex(const std::size_t index)
{
    switch (index) {
    case 0:
        m_viewport.setMode(EditorViewportMode::Edit);
        m_runState = RunState::Stopped;
        break;
    case 1:
        m_viewport.setMode(EditorViewportMode::Play);
        m_runState = RunState::Playing;
        break;
    case 2:
        m_viewport.setMode(EditorViewportMode::Simulate);
        m_runState = RunState::Playing;
        break;
    case 3:
        m_viewport.setMode(EditorViewportMode::HudEdit);
        m_runState = RunState::Stopped;
        break;
    default:
        break;
    }
}

void EditorUI::openSpriteFileDialog()
{
    FileDialogOptions options;
    options.title = "Load Sprite";
    options.filters = {
        {"Image files", {"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tga"}},
        {"All files", {"*.*"}},
    };

    if (!m_spritePath.empty()) {
        const std::filesystem::path currentPath = m_spritePath;
        options.initialDirectory = currentPath.has_parent_path() ? currentPath.parent_path() : std::filesystem::current_path();
    }

    if (const auto selectedPath = FileDialog::openFile(options)) {
        m_spritePath = selectedPath->string();
        m_worldDebug.setSpritePath(m_spritePath);
        m_worldDebug.addSprite();
        spdlog::info("Editor UI: Sprite test path set to '{}'.", m_spritePath);
    }
}

const UIRect& EditorUI::viewportBounds() const
{
    return m_viewportBounds;
}

} // namespace Engine
