#include "Engine/Editor/EditorUI.hpp"

#include "Engine/Core/Input.hpp"
#include "Engine/Renderer/Renderer2D.hpp"
#include "Engine/Renderer/TextRenderer.hpp"
#include "Engine/UI/UIFrame.hpp"
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

bool containsPoint(const UIRect& rect, const glm::vec2& point)
{
    return point.x >= rect.position.x &&
        point.y >= rect.position.y &&
        point.x <= rect.position.x + rect.size.x &&
        point.y <= rect.position.y + rect.size.y;
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

std::size_t blendModeIndex(const WorldBlendMode mode)
{
    switch (mode) {
    case WorldBlendMode::Opaque:
        return 0U;
    case WorldBlendMode::Alpha:
        return 1U;
    case WorldBlendMode::Additive:
        return 2U;
    }

    return 1U;
}

WorldBlendMode blendModeFromIndex(const std::size_t index)
{
    switch (index) {
    case 0U:
        return WorldBlendMode::Opaque;
    case 2U:
        return WorldBlendMode::Additive;
    default:
        return WorldBlendMode::Alpha;
    }
}

std::size_t samplerModeIndex(const WorldSamplerMode mode)
{
    switch (mode) {
    case WorldSamplerMode::Nearest:
        return 0U;
    case WorldSamplerMode::Linear:
        return 1U;
    }

    return 1U;
}

WorldSamplerMode samplerModeFromIndex(const std::size_t index)
{
    return index == 0U ? WorldSamplerMode::Nearest : WorldSamplerMode::Linear;
}

} // namespace

EditorUI::EditorUI(EditorViewport& viewport, Scene& scene, EditorSelectionState& selection)
    : m_viewport(viewport)
    , m_scene(scene)
    , m_selection(selection)
{
    std::string styleError;
    if (!UIStyleParser::loadFile(NIKREON_ASSET_DIR "/styles/editor.ui.css", m_style, styleError)) {
        spdlog::warn("Editor UI stylesheet was not loaded: {}", styleError);
    }
}

void EditorUI::render(Renderer2D& renderer2D, TextRenderer& textRenderer, ResourceManager& resources, const glm::uvec2& viewportSize, const Input& input)
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

    if (m_textureAssetsDirty) {
        refreshTextureAssets(resources);
    }

    m_ui.begin(m_context, renderer2D, textRenderer, m_style, {{0.0f, 0.0f}, {width, height}});
    declareUI(width, height, resources);
    m_ui.layout();
    syncBuilderBounds();
    handleAssetContextActions(resources, input);
    if (updatePanelSplitters()) {
        declareUI(width, height, resources);
        m_ui.layout();
        syncBuilderBounds();
        handleAssetContextActions(resources, input);
    }
    updateAssetContextMenu(resources, input);
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

    const glm::vec4 viewportBorder = m_viewport.focused() ? m_editorStyle.viewportFocusedBorder : m_editorStyle.viewportBorder;
    renderer2D.drawRect(m_viewportBounds.position, m_viewportBounds.size, viewportBorder, 2.0f);

    renderPanelSplitters(renderer2D);
    m_ui.render();
    renderAssetPreviews(resources, renderer2D, textRenderer);
    renderAssetContextMenu(renderer2D, textRenderer);

    m_context.endFrame();
    m_ui.end();
}

void EditorUI::update(const float deltaTime)
{
    if (deltaTime > 0.0f && std::isfinite(deltaTime)) {
        m_fpsAccumulatedTime += deltaTime;
        ++m_fpsFrameCount;

        if (m_fpsAccumulatedTime >= 0.25f) {
            m_displayFps = static_cast<float>(m_fpsFrameCount) / m_fpsAccumulatedTime;
            m_fpsAccumulatedTime = 0.0f;
            m_fpsFrameCount = 0;
        }
    }
}

void EditorUI::declareUI(const float width, const float height, ResourceManager& resources)
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
    SceneObject* selectedObject = selectedSceneObject();

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
        .text("A")
        .textStyle("toolbar-toggle")
        .tooltip("Toggle assets")
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

    m_ui.panel("toolbar.spacer")
        .parent("toolbar")
        .drawBackground(false)
        .grow(1.0f);

    m_ui.label("toolbar.fps")
        .parent("toolbar")
        .text(fpsCounterText())
        .textStyle("fps-counter")
        .width(92.0f)
        .height(24.0f);

    m_ui.panel("console")
        .dock(UIDock::Bottom)
        .height(m_consoleHeight)
        .visible(m_consoleVisible)
        .vertical()
        .padding({16.0f, 8.0f, 16.0f, 16.0f})
        .gap(8.0f);

    m_ui.panel("assets.header")
        .parent("console")
        .drawBackground(false)
        .horizontal()
        .height(28.0f)
        .padding(UIEdgeInsets::all(0.0f))
        .gap(8.0f);

    m_ui.label("assets.title")
        .parent("assets.header")
        .text("Assets")
        .textStyle("heading")
        .width(90.0f)
        .height(22.0f);

    m_ui.button("assets.refresh")
        .parent("assets.header")
        .styleClass("toolbar")
        .text("Refresh")
        .textStyle("toolbar-toggle")
        .width(92.0f)
        .height(24.0f)
        .onClick([this, &resources]() {
            refreshTextureAssets(resources);
            spdlog::info("Editor UI: refreshed texture asset list.");
        });

    const std::size_t visibleAssetCount = std::min(m_textureAssets.size(), std::size_t{12});
    m_ui.panel("assets.list")
        .parent("console")
        .drawBackground(false)
        .grid(4)
        .height(std::max(28.0f, m_consoleHeight - 62.0f))
        .padding(UIEdgeInsets::all(0.0f))
        .gap(6.0f);

    if (visibleAssetCount == 0U) {
        m_ui.label("assets.empty")
            .parent("assets.list")
            .text("No PNG/JPG assets found")
            .textStyle("muted")
            .height(22.0f);
    }

    for (std::size_t index = 0; index < visibleAssetCount; ++index) {
        const TextureAssetInfo& asset = m_textureAssets[index];
        const std::string id = "assets.texture." + std::to_string(index);
        const bool selected =
            selectedObject != nullptr &&
            selectedObject->sprite2D &&
            std::filesystem::path{selectedObject->sprite2D->texturePath}.lexically_normal() == asset.path.lexically_normal();

        m_ui.button(id)
            .parent("assets.list")
            .styleClass("hierarchy-row")
            .text("")
            .textStyle("hierarchy-row")
            .height(62.0f)
            .selected(selected);
    }

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
        .checked(m_viewport.gridVisible())
        .onCheckedChanged([this](const bool enabled) {
            m_viewport.setGridVisible(enabled);
            spdlog::info("Editor UI: Viewport grid {}.", enabled ? "enabled" : "disabled");
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

    if (selectedObject != nullptr) {
        m_ui.label("sceneObject.title")
            .parent("inspector")
            .text(selectedObject->hasSprite2D() ? "Selected Sprite" : "Selected Object")
            .textStyle("heading")
            .height(18.0f);

        m_ui.textInput("sceneObject.name")
            .parent("inspector")
            .styleClass("inspector")
            .label("Name")
            .labelStyle("control-label")
            .value(selectedObject->name)
            .placeholder("Object name")
            .onTextChanged([selectedObject](const std::string_view value) { selectedObject->name = std::string(value); });

        m_ui.panel("sceneObject.position-grid")
            .parent("inspector")
            .drawBackground(false)
            .grid(3)
            .height(28.0f)
            .padding(UIEdgeInsets::all(0.0f))
            .gap(6.0f);

        m_ui.numberFloat("sceneObject.position.x")
            .parent("sceneObject.position-grid")
            .styleClass("plain")
            .value(selectedObject->transform.position.x)
            .range(-10000.0f, 10000.0f)
            .precision(2)
            .sensitivity(0.1f)
            .onChanged([selectedObject](const float value) { selectedObject->transform.position.x = value; });

        m_ui.numberFloat("sceneObject.position.y")
            .parent("sceneObject.position-grid")
            .styleClass("plain")
            .value(selectedObject->transform.position.y)
            .range(-10000.0f, 10000.0f)
            .precision(2)
            .sensitivity(0.1f)
            .onChanged([selectedObject](const float value) { selectedObject->transform.position.y = value; });

        m_ui.numberFloat("sceneObject.position.z")
            .parent("sceneObject.position-grid")
            .styleClass("plain")
            .value(selectedObject->transform.position.z)
            .range(-10000.0f, 10000.0f)
            .precision(2)
            .sensitivity(0.1f)
            .onChanged([selectedObject](const float value) { selectedObject->transform.position.z = value; });

        m_ui.panel("sceneObject.scale-grid")
            .parent("inspector")
            .drawBackground(false)
            .grid(2)
            .height(28.0f)
            .padding(UIEdgeInsets::all(0.0f))
            .gap(6.0f);

        m_ui.numberFloat("sceneObject.scale.x")
            .parent("sceneObject.scale-grid")
            .styleClass("plain")
            .value(selectedObject->transform.scale.x)
            .range(0.01f, 100.0f)
            .precision(2)
            .sensitivity(0.05f)
            .onChanged([selectedObject](const float value) { selectedObject->transform.scale.x = value; });

        m_ui.numberFloat("sceneObject.scale.y")
            .parent("sceneObject.scale-grid")
            .styleClass("plain")
            .value(selectedObject->transform.scale.y)
            .range(0.01f, 100.0f)
            .precision(2)
            .sensitivity(0.05f)
            .onChanged([selectedObject](const float value) { selectedObject->transform.scale.y = value; });

        if (selectedObject->sprite2D) {
            Sprite2DComponent* sprite = &*selectedObject->sprite2D;

            m_ui.filePathInput("sceneObject.spritePath")
                .parent("inspector")
                .styleClass("inspector")
                .label("Texture")
                .labelStyle("control-label")
                .value(sprite->texturePath)
                .buttonLabel("Browse")
                .browseButtonWidth(78.0f)
                .onTextChanged([sprite](const std::string_view value) { sprite->texturePath = std::string(value); });

            m_ui.panel("sceneObject.size-grid")
                .parent("inspector")
                .drawBackground(false)
                .grid(2)
                .height(28.0f)
                .padding(UIEdgeInsets::all(0.0f))
                .gap(6.0f);

            m_ui.numberFloat("sceneObject.size.x")
                .parent("sceneObject.size-grid")
                .styleClass("plain")
                .value(sprite->size.x)
                .range(1.0f, 10000.0f)
                .precision(2)
                .sensitivity(0.5f)
                .onChanged([sprite](const float value) { sprite->size.x = value; });

            m_ui.numberFloat("sceneObject.size.y")
                .parent("sceneObject.size-grid")
                .styleClass("plain")
                .value(sprite->size.y)
                .range(1.0f, 10000.0f)
                .precision(2)
                .sensitivity(0.5f)
                .onChanged([sprite](const float value) { sprite->size.y = value; });

            m_ui.colorPicker("sceneObject.tint")
                .parent("inspector")
                .label("Tint")
                .labelStyle("control-label")
                .color(sprite->tint)
                .onColorChanged([sprite](const glm::vec4& color) { sprite->tint = color; });

            m_ui.dropdown("sceneObject.blendMode")
                .parent("inspector")
                .styleClass("inspector")
                .label("Blend")
                .labelStyle("control-label")
                .items({"Opaque", "Alpha", "Additive"})
                .selectedIndex(blendModeIndex(sprite->renderState.blendMode))
                .onSelectionChanged([sprite](const std::size_t index, std::string_view) {
                    sprite->renderState.blendMode = blendModeFromIndex(index);
                });

            m_ui.dropdown("sceneObject.samplerMode")
                .parent("inspector")
                .styleClass("inspector")
                .label("Sampler")
                .labelStyle("control-label")
                .items({"Nearest", "Linear"})
                .selectedIndex(samplerModeIndex(sprite->renderState.samplerMode))
                .onSelectionChanged([sprite](const std::size_t index, std::string_view) {
                    sprite->renderState.samplerMode = samplerModeFromIndex(index);
                });
        }
    }

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

void EditorUI::handleAssetContextActions(ResourceManager&, const Input& input)
{
    const bool rightMousePressed = input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
    const bool rightClick = rightMousePressed && !m_rightMouseWasPressed;
    m_rightMouseWasPressed = rightMousePressed;

    if (!rightClick) {
        return;
    }

    const std::size_t visibleAssetCount = std::min(m_textureAssets.size(), std::size_t{12});
    const glm::vec2 mouse = input.mousePosition();
    for (std::size_t index = 0; index < visibleAssetCount; ++index) {
        const std::string id = "assets.texture." + std::to_string(index);
        const UIRect bounds = m_ui.bounds(id);

        if (containsPoint(bounds, mouse)) {
            m_assetContextMenuOpen = true;
            m_assetContextAssetIndex = index;
            m_assetContextMenuPosition = mouse;
            return;
        }
    }

    m_assetContextMenuOpen = false;
}

UIRect EditorUI::assetContextMenuBounds() const
{
    const glm::vec2 size{156.0f, 28.0f};
    glm::vec2 position = m_assetContextMenuPosition + glm::vec2{6.0f, 6.0f};

    position.x = std::clamp(position.x, 4.0f, std::max(4.0f, m_renderSize.x - size.x - 4.0f));
    position.y = std::clamp(position.y, 4.0f, std::max(4.0f, m_renderSize.y - size.y - 4.0f));

    return {position, size};
}

void EditorUI::updateAssetContextMenu(ResourceManager& resources, const Input& input)
{
    if (!m_assetContextMenuOpen) {
        return;
    }

    if (m_assetContextAssetIndex >= m_textureAssets.size()) {
        m_assetContextMenuOpen = false;
        return;
    }

    const UIRect menuBounds = assetContextMenuBounds();
    m_context.registerLayer("assets.contextMenu", 160, {menuBounds.position, menuBounds.size}, false);
    m_context.pushLayer("assets.contextMenu");

    constexpr float rowHeight = 28.0f;
    const glm::vec2 rowSize{menuBounds.size.x, rowHeight};
    const glm::vec2 addPosition{menuBounds.position.x, menuBounds.position.y};

    if (m_context.button("assets.contextMenu.add", addPosition, rowSize)) {
        createSceneSpriteFromAsset(m_textureAssets[m_assetContextAssetIndex], resources);
        m_assetContextMenuOpen = false;
    }

    m_context.popLayer();

    if (m_assetContextMenuOpen && m_context.primaryMousePressed() && !containsPoint(menuBounds, input.mousePosition())) {
        m_assetContextMenuOpen = false;
    }
}

void EditorUI::renderAssetPreviews(ResourceManager& resources, Renderer2D& renderer2D, TextRenderer& textRenderer)
{
    const UITextStyle& labelText = m_style.resolveText("hierarchy-row");
    const UITextStyle& mutedText = m_style.resolveText("muted");
    const std::size_t visibleAssetCount = std::min(m_textureAssets.size(), std::size_t{12});
    UIFrame frame{m_context, renderer2D, textRenderer, m_style};
    UICompositeRenderScope previewRenderScope{frame};

    struct PreviewItem {
        const TextureAssetInfo* asset{nullptr};
        UIRect bounds{};
        glm::vec2 previewPosition{0.0f};
        glm::vec2 previewSize{48.0f};
    };

    std::vector<PreviewItem> items;
    items.reserve(visibleAssetCount);

    for (std::size_t index = 0; index < visibleAssetCount; ++index) {
        const TextureAssetInfo& asset = m_textureAssets[index];
        const std::string id = "assets.texture." + std::to_string(index);
        const UIRect bounds = m_ui.bounds(id);
        if (bounds.size.x <= 0.0f || bounds.size.y <= 0.0f) {
            continue;
        }

        items.push_back({
            &asset,
            bounds,
            bounds.position + glm::vec2{6.0f, 6.0f},
            {48.0f, 48.0f},
        });
    }

    for (const PreviewItem& item : items) {
        renderer2D.drawSdfRect(item.previewPosition, item.previewSize, 3.0f, m_style.field.fill, m_style.field.border, 1.0f);
    }

    for (const PreviewItem& item : items) {
        const TextureAssetInfo& asset = *item.asset;

        if (asset.handle) {
            const TextureResource* texture = resources.tryTexture(asset.handle);
            if (texture != nullptr && !texture->pixels.rgba8.empty()) {
                renderer2D.uploadImage(
                    asset.handle.value(),
                    texture->pixels.width,
                    texture->pixels.height,
                    texture->pixels.rgba8.data(),
                    texture->pixels.rgba8.size());
                renderer2D.drawImage(asset.handle.value(), item.previewPosition, item.previewSize);
            }
        }
    }

    for (const PreviewItem& item : items) {
        if (item.asset->handle) {
            renderer2D.drawRect(item.previewPosition, item.previewSize, m_style.field.border, 1.0f);
        }
    }

    for (const PreviewItem& item : items) {
        const TextureAssetInfo& asset = *item.asset;
        std::string label = asset.displayPath;
        if (label.size() > 34U) {
            label = label.substr(0U, 31U) + "...";
        }

        const glm::vec2 labelPosition = item.bounds.position + glm::vec2{64.0f, 11.0f};
        textRenderer.drawText(label, labelPosition, labelText.color, labelText.font, labelText.scale);

        std::string sizeText = asset.loaded ? "Loaded" : "Missing";
        if (asset.handle) {
            if (const TextureResource* texture = resources.tryTexture(asset.handle)) {
                sizeText = texture->fallback == TextureFallbackKind::Missing
                    ? "Missing"
                    : std::to_string(texture->pixels.width) + " x " + std::to_string(texture->pixels.height);
            }
        }
        textRenderer.drawText(sizeText, labelPosition + glm::vec2{0.0f, 20.0f}, mutedText.color, mutedText.font, mutedText.scale);
    }
}

void EditorUI::renderAssetContextMenu(Renderer2D& renderer2D, TextRenderer& textRenderer)
{
    if (!m_assetContextMenuOpen || m_assetContextAssetIndex >= m_textureAssets.size()) {
        return;
    }

    const UIRect menuBounds = assetContextMenuBounds();

    const UIButtonStyle& buttonStyle = m_style.resolveButton("toolbar");
    const UITextStyle& buttonText = m_style.resolveText("toolbar-toggle");
    UIFrame frame{m_context, renderer2D, textRenderer, m_style};
    UICompositeRenderScope menuRenderScope{frame};

    renderer2D.drawQuad(menuBounds.position, menuBounds.size, m_style.panel.fill);
    renderer2D.drawRect(menuBounds.position, menuBounds.size, m_style.panel.border, 1.0f);

    const auto drawMenuRow = [&](const std::string_view id, const std::string_view label, const std::size_t rowIndex) {
        constexpr float rowHeight = 28.0f;
        const glm::vec2 position{menuBounds.position.x, menuBounds.position.y + static_cast<float>(rowIndex) * rowHeight};
        const glm::vec2 size{menuBounds.size.x, rowHeight};
        const glm::vec4 fill = m_context.isActive(id)
            ? buttonStyle.pressed
            : m_context.isHot(id)
                ? buttonStyle.hovered
                : glm::vec4{0.0f, 0.0f, 0.0f, 0.0f};

        if (fill.a > 0.0f) {
            renderer2D.drawQuad(position, size, fill);
        }

        frame.drawText(label, {position + glm::vec2{10.0f, 0.0f}, {std::max(size.x - 20.0f, 0.0f), size.y}}, buttonText);
    };

    drawMenuRow("assets.contextMenu.add", "Add To Scene", 0U);
}

void EditorUI::createSceneSpriteFromAsset(const TextureAssetInfo& asset, ResourceManager& resources)
{
    const std::filesystem::path texturePath = asset.path;
    const std::string displayPath = asset.displayPath;
    const TextureHandle handle = resources.loadTexture(texturePath);

    SceneObject& object = m_scene.createSprite2D(
        "Sprite " + std::to_string(m_scene.sprite2DCount() + 1U),
        texturePath.string());

    object.transform.position = {0.0f, 0.0f, 0.0f};

    if (handle) {
        const TextureResource& texture = resources.texture(handle);
        const float width = static_cast<float>(std::max(texture.pixels.width, 1U));
        const float height = static_cast<float>(std::max(texture.pixels.height, 1U));
        const float maxDimension = std::max(width, height);
        const float scale = maxDimension > 128.0f ? 128.0f / maxDimension : 1.0f;

        if (object.sprite2D) {
            object.sprite2D->size = {width * scale, height * scale};
        }
    }

    m_selection.select(object.id);
    refreshTextureAssets(resources);

    spdlog::info("Editor UI: added scene sprite from asset '{}'.", displayPath);
}

void EditorUI::refreshTextureAssets(ResourceManager& resources)
{
    m_textureAssets = resources.scanTextureAssets(NIKREON_ASSET_DIR);
    for (TextureAssetInfo& asset : m_textureAssets) {
        const TextureHandle handle = resources.loadTexture(asset.path);
        if (handle) {
            asset.loaded = handle != resources.missingTexture();
            asset.handle = handle;
        }
    }
    m_textureAssetsDirty = false;
}

SceneObject* EditorUI::selectedSceneObject()
{
    const SceneObjectId selectedObjectId = m_selection.selectedSceneObjectId();
    if (selectedObjectId == InvalidSceneObjectId) {
        return nullptr;
    }

    SceneObject* object = m_scene.findObject(selectedObjectId);
    if (object == nullptr) {
        m_selection.clear();
    }
    return object;
}

std::string EditorUI::fpsCounterText() const
{
    if (m_displayFps <= 0.0f) {
        return "-- FPS";
    }

    std::ostringstream text;
    text << std::fixed << std::setprecision(0) << m_displayFps << " FPS";
    return text.str();
}

const UIRect& EditorUI::viewportBounds() const
{
    return m_viewportBounds;
}

} // namespace Engine
