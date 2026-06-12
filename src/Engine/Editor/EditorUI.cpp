#include "Engine/Editor/EditorUI.hpp"

#include "Engine/Core/FileDialog.hpp"
#include "Engine/Core/Input.hpp"
#include "Engine/Core/Window.hpp"
#include "Engine/Renderer/Renderer2D.hpp"
#include "Engine/Renderer/TextRenderer.hpp"
#include "Engine/UI/UIFrame.hpp"
#include "Engine/UI/UIStyleParser.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <GLFW/glfw3.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace Engine {

#ifndef NIKREON_ASSET_DIR
#define NIKREON_ASSET_DIR "assets"
#endif

namespace {

constexpr float Pi = 3.14159265358979323846f;

float radiansToDegrees(const float radians)
{
    return radians * 180.0f / Pi;
}

float degreesToRadians(const float degrees)
{
    return degrees * Pi / 180.0f;
}

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

std::string_view blendModeName(const WorldBlendMode mode)
{
    switch (mode) {
    case WorldBlendMode::Opaque:
        return "Opaque";
    case WorldBlendMode::Alpha:
        return "Alpha";
    case WorldBlendMode::Additive:
        return "Additive";
    }

    return "Alpha";
}

std::string_view samplerModeName(const WorldSamplerMode mode)
{
    switch (mode) {
    case WorldSamplerMode::Nearest:
        return "Nearest";
    case WorldSamplerMode::Linear:
        return "Linear";
    }

    return "Linear";
}

void applyUiCursorRequest(const UIContext& context, const Input& input)
{
    if (const glm::vec2* requestedMousePosition = context.requestedMousePosition()) {
        input.setMousePosition(*requestedMousePosition);
    }

    switch (context.requestedCursor()) {
    case UICursor::Text:
        input.setCursorVisible(true);
        input.setCursorShape(CursorShape::Text);
        break;
    case UICursor::ResizeHorizontal:
        input.setCursorVisible(true);
        input.setCursorShape(CursorShape::ResizeHorizontal);
        break;
    case UICursor::Hidden:
        input.setCursorShape(CursorShape::ResizeHorizontal);
        input.setCursorVisible(false);
        break;
    case UICursor::Arrow:
        input.setCursorVisible(true);
        input.setCursorShape(CursorShape::Arrow);
        break;
    }
}

std::string truncateAssetLabel(const std::string& label, const std::size_t maxCharacters)
{
    if (label.size() <= maxCharacters) {
        return label;
    }

    if (maxCharacters <= 3U) {
        return label.substr(0U, maxCharacters);
    }

    return label.substr(0U, maxCharacters - 3U) + "...";
}

std::string assetTileLabel(const std::string& label)
{
    const std::filesystem::path path{label};
    const std::string filename = path.filename().generic_string();
    if (!filename.empty()) {
        return truncateAssetLabel(filename, 44U);
    }

    return truncateAssetLabel(label, 44U);
}

void drawWrappedTextClipped(
    TextRenderer& textRenderer,
    std::string_view text,
    const UIRect& bounds,
    const UITextStyle& style,
    const TextAlignment alignment = TextAlignment::Left,
    const bool wordWrap = true)
{
    if (bounds.size.x <= 0.0f || bounds.size.y <= 0.0f || text.empty()) {
        return;
    }

    constexpr float textClipTopPadding = 1.0f;
    constexpr float textClipBottomPadding = 3.0f;
    const UIClipRect clipBounds{
        bounds.position - glm::vec2{0.0f, textClipTopPadding},
        bounds.size + glm::vec2{0.0f, textClipTopPadding + textClipBottomPadding},
    };

    textRenderer.pushClipRect(clipBounds);
    textRenderer.drawText(
        text,
        bounds.position,
        style.color,
        style.font,
        style.scale,
        alignment,
        {
            bounds.size.x,
            1.0f,
            wordWrap,
        });
    textRenderer.popClipRect();
}

int assetGridColumnsForWidth(const float width)
{
    return std::clamp(static_cast<int>(width / 280.0f), 1, 5);
}

std::string modelBoundsText(const MeshBounds& bounds)
{
    if (!bounds.valid) {
        return "Bounds unavailable";
    }

    const glm::vec3 size = bounds.maximum - bounds.minimum;
    std::ostringstream text;
    text << std::fixed << std::setprecision(1)
         << "Bounds "
         << size.x << " x "
         << size.y << " x "
         << size.z;
    return text.str();
}

std::string assetDisplayPath(const std::filesystem::path& path)
{
    std::error_code error;
    const std::filesystem::path assetRoot = std::filesystem::absolute(NIKREON_ASSET_DIR, error);
    if (!error) {
        std::filesystem::path relativePath = std::filesystem::relative(path, assetRoot, error);
        const std::string relativeText = relativePath.generic_string();
        if (!error && !relativeText.empty() && relativeText.rfind("..", 0U) != 0U) {
            return relativeText;
        }
    }

    return path.filename().generic_string();
}

bool isPathInsideAssets(const std::filesystem::path& path)
{
    std::error_code error;
    const std::filesystem::path assetRoot = std::filesystem::absolute(NIKREON_ASSET_DIR, error);
    if (error) {
        return false;
    }

    const std::filesystem::path relativePath = std::filesystem::relative(path, assetRoot, error);
    const std::string relativeText = relativePath.generic_string();
    return !error && !relativeText.empty() && relativeText.rfind("..", 0U) != 0U;
}

std::string lowercaseExtension(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return extension;
}

bool gltfUriIsExternalFile(const std::string& uri)
{
    return !uri.empty() &&
        uri.find(':') == std::string::npos &&
        uri.rfind("data:", 0U) != 0U;
}

void collectGltfUrisFromArray(
    const nlohmann::json& document,
    const char* key,
    std::set<std::filesystem::path>& relativePaths)
{
    const auto found = document.find(key);
    if (found == document.end() || !found->is_array()) {
        return;
    }

    for (const nlohmann::json& item : *found) {
        const auto uri = item.find("uri");
        if (uri == item.end() || !uri->is_string()) {
            continue;
        }

        const std::string uriText = uri->get<std::string>();
        if (gltfUriIsExternalFile(uriText)) {
            relativePaths.insert(std::filesystem::path{uriText}.lexically_normal());
        }
    }
}

std::set<std::filesystem::path> gltfSidecarPaths(const std::filesystem::path& gltfPath)
{
    std::set<std::filesystem::path> relativePaths;
    if (lowercaseExtension(gltfPath) != ".gltf") {
        return relativePaths;
    }

    std::ifstream input{gltfPath};
    if (!input) {
        return relativePaths;
    }

    try {
        const nlohmann::json document = nlohmann::json::parse(input);
        collectGltfUrisFromArray(document, "buffers", relativePaths);
        collectGltfUrisFromArray(document, "images", relativePaths);
    } catch (const std::exception& exception) {
        spdlog::warn("Editor UI: failed to inspect glTF sidecar files for '{}': {}.", gltfPath.string(), exception.what());
    }

    return relativePaths;
}

void copyGltfSidecars(
    const std::filesystem::path& sourceGltf,
    const std::filesystem::path& targetDirectory)
{
    const std::set<std::filesystem::path> sidecars = gltfSidecarPaths(sourceGltf);
    for (const std::filesystem::path& relativeSidecar : sidecars) {
        std::error_code error;
        const std::filesystem::path sourceSidecar = (sourceGltf.parent_path() / relativeSidecar).lexically_normal();
        if (!std::filesystem::is_regular_file(sourceSidecar, error)) {
            spdlog::warn("Editor UI: glTF sidecar file was not found: '{}'.", sourceSidecar.string());
            continue;
        }

        const std::filesystem::path targetSidecar = (targetDirectory / relativeSidecar).lexically_normal();
        std::filesystem::create_directories(targetSidecar.parent_path(), error);
        if (error) {
            spdlog::warn("Editor UI: failed to create glTF sidecar directory '{}': {}.", targetSidecar.parent_path().string(), error.message());
            continue;
        }

        if (std::filesystem::equivalent(sourceSidecar, targetSidecar, error)) {
            error.clear();
            continue;
        }

        std::filesystem::copy_file(sourceSidecar, targetSidecar, std::filesystem::copy_options::overwrite_existing, error);
        if (error) {
            spdlog::warn("Editor UI: failed to copy glTF sidecar '{}' to '{}': {}.", sourceSidecar.string(), targetSidecar.string(), error.message());
        }
    }

}

std::optional<std::filesystem::path> importAssetFile(const std::filesystem::path& sourcePath, const std::filesystem::path& assetSubdirectory)
{
    std::error_code error;
    const std::filesystem::path absoluteSource = std::filesystem::absolute(sourcePath, error);
    if (error || !std::filesystem::is_regular_file(absoluteSource, error)) {
        spdlog::warn("Editor UI: asset import source is not a file: '{}'.", sourcePath.string());
        return std::nullopt;
    }

    if (isPathInsideAssets(absoluteSource)) {
        return absoluteSource;
    }

    const std::filesystem::path targetDirectory = std::filesystem::path{NIKREON_ASSET_DIR} / assetSubdirectory;
    std::filesystem::create_directories(targetDirectory, error);
    if (error) {
        spdlog::warn("Editor UI: failed to create asset import directory '{}': {}.", targetDirectory.string(), error.message());
        return std::nullopt;
    }

    const std::filesystem::path sourceFilename = absoluteSource.filename();
    const std::filesystem::path sourceStem = sourceFilename.stem();
    const std::filesystem::path sourceExtension = sourceFilename.extension();
    std::filesystem::path targetPath = targetDirectory / sourceFilename;

    for (std::uint32_t copyIndex = 1U; std::filesystem::exists(targetPath, error); ++copyIndex) {
        targetPath = targetDirectory / (sourceStem.string() + "_" + std::to_string(copyIndex) + sourceExtension.string());
        error.clear();
    }

    std::filesystem::copy_file(absoluteSource, targetPath, std::filesystem::copy_options::none, error);
    if (error) {
        spdlog::warn("Editor UI: failed to copy asset '{}' to '{}': {}.", absoluteSource.string(), targetPath.string(), error.message());
        return std::nullopt;
    }

    if (lowercaseExtension(absoluteSource) == ".gltf") {
        copyGltfSidecars(absoluteSource, targetDirectory);
    }

    return std::filesystem::absolute(targetPath, error);
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

    if (m_assetsDirty) {
        refreshAssets(resources);
    }

    m_ui.begin(m_context, renderer2D, textRenderer, m_style, {{0.0f, 0.0f}, {width, height}});
    declareUI(width, height, resources);
    m_ui.layout();
    syncBuilderBounds();
    const bool assetContextMenuWasOpen = m_assetContextMenuOpen;
    handleAssetContextActions(resources, input);
    if (assetContextMenuWasOpen != m_assetContextMenuOpen) {
        declareUI(width, height, resources);
        m_ui.layout();
        syncBuilderBounds();
    }
    if (updatePanelSplitters()) {
        declareUI(width, height, resources);
        m_ui.layout();
        syncBuilderBounds();
        handleAssetContextActions(resources, input);
    }
    updateAssetContextMenu(resources, input);
    m_ui.update();
    applyUiCursorRequest(m_context, input);

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
    m_ui.renderBaseLayer();
    renderAssetPreviews(resources, renderer2D, textRenderer);
    renderAssetContextMenu(renderer2D, textRenderer);
    m_ui.renderTopLayer();

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
    m_consoleHeight = std::clamp(m_consoleHeight, 116.0f, 360.0f);

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
        .text(editorViewportModeName(m_viewport.mode()))
        .popupSize({142.0f, 144.0f})
        .padding(UIEdgeInsets::all(8.0f))
        .gap(4.0f);

    const auto addViewportModeOption = [this](const std::string_view id, const std::size_t index, const EditorViewportMode mode) {
        m_ui.button(id)
            .parent("toolbar.viewportMode")
            .styleClass("hierarchy-row")
            .text(editorViewportModeName(mode))
            .textStyle("hierarchy-row")
            .height(28.0f)
            .selected(m_viewport.mode() == mode)
            .onClick([this, index]() {
                setViewportModeFromIndex(index);
                m_ui.closeDropdown("toolbar.viewportMode");
            });
    };

    addViewportModeOption("toolbar.viewportMode.edit", 0U, EditorViewportMode::Edit);
    addViewportModeOption("toolbar.viewportMode.play", 1U, EditorViewportMode::Play);
    addViewportModeOption("toolbar.viewportMode.simulate", 2U, EditorViewportMode::Simulate);
    addViewportModeOption("toolbar.viewportMode.hudEdit", 3U, EditorViewportMode::HudEdit);

    const std::string viewOptionsLabel = std::string(editorViewOrientationName(m_viewport.viewOrientation())) + " v";
    m_ui.dropdown("toolbar.viewOptions")
        .parent("toolbar")
        .styleClass("toolbar")
        .textStyle("toolbar-toggle")
        .width(132.0f)
        .text(viewOptionsLabel)
        .popupSize({360.0f, 420.0f})
        .padding(UIEdgeInsets::all(12.0f))
        .gap(6.0f);

    m_ui.label("toolbar.viewOptions.viewSection")
        .parent("toolbar.viewOptions")
        .text("VIEW")
        .textStyle("muted")
        .height(18.0f);

    const auto addViewOption = [this](const std::string_view id, const EditorViewOrientation orientation) {
        m_ui.button(id)
            .parent("toolbar.viewOptions")
            .styleClass("hierarchy-row")
            .text(editorViewOrientationName(orientation))
            .textStyle("hierarchy-row")
            .height(28.0f)
            .selected(m_viewport.viewOrientation() == orientation)
            .onClick([this, orientation]() {
                m_viewport.setViewOrientation(orientation);
                m_ui.closeDropdown("toolbar.viewOptions");
            });
    };

    addViewOption("toolbar.viewOptions.perspective", EditorViewOrientation::Perspective);
    addViewOption("toolbar.viewOptions.top", EditorViewOrientation::Top);
    addViewOption("toolbar.viewOptions.bottom", EditorViewOrientation::Bottom);
    addViewOption("toolbar.viewOptions.front", EditorViewOrientation::Front);
    addViewOption("toolbar.viewOptions.back", EditorViewOrientation::Back);
    addViewOption("toolbar.viewOptions.left", EditorViewOrientation::Left);
    addViewOption("toolbar.viewOptions.right", EditorViewOrientation::Right);

    m_ui.label("toolbar.viewOptions.cameraSection")
        .parent("toolbar.viewOptions")
        .text("CAMERA")
        .textStyle("muted")
        .height(18.0f);

    const EditorViewportCameraSettings cameraSettings = m_viewport.cameraSettings();
    const auto addCameraRow = [this](const std::string_view id, const std::string_view label) {
        m_ui.panel(id)
            .parent("toolbar.viewOptions")
            .drawBackground(false)
            .horizontal()
            .height(32.0f)
            .padding(UIEdgeInsets::all(0.0f))
            .gap(8.0f);

        m_ui.label(std::string(id) + ".label")
            .parent(id)
            .text(label)
            .textStyle("toolbar-toggle")
            .width(178.0f)
            .height(28.0f);
    };

    addCameraRow("toolbar.viewOptions.fovRow", "Field Of View");
    m_ui.numberFloat("toolbar.viewOptions.fov")
        .parent("toolbar.viewOptions.fovRow")
        .styleClass("plain")
        .value(cameraSettings.verticalFovDegrees)
        .range(5.0f, 170.0f)
        .precision(1)
        .sensitivity(0.2f)
        .grow(1.0f)
        .height(26.0f)
        .onChanged([this](const float value) {
            EditorViewportCameraSettings settings = m_viewport.cameraSettings();
            settings.verticalFovDegrees = value;
            m_viewport.setCameraSettings(settings);
        });

    addCameraRow("toolbar.viewOptions.nearRow", "Near View Plane");
    m_ui.numberFloat("toolbar.viewOptions.near")
        .parent("toolbar.viewOptions.nearRow")
        .styleClass("plain")
        .value(cameraSettings.nearPlane)
        .range(0.001f, 1000000.0f)
        .precision(3)
        .sensitivity(0.01f)
        .grow(1.0f)
        .height(26.0f)
        .onChanged([this](const float value) {
            EditorViewportCameraSettings settings = m_viewport.cameraSettings();
            settings.nearPlane = value;
            m_viewport.setCameraSettings(settings);
        });

    addCameraRow("toolbar.viewOptions.farRow", "Far View Plane");
    if (cameraSettings.infiniteFarPlane) {
        m_ui.button("toolbar.viewOptions.farInfinity")
            .parent("toolbar.viewOptions.farRow")
            .styleClass("toolbar")
            .text("Infinity")
            .textStyle("toolbar-toggle")
            .grow(1.0f)
            .height(26.0f)
            .selected(true)
            .onClick([this]() {
                EditorViewportCameraSettings settings = m_viewport.cameraSettings();
                settings.infiniteFarPlane = false;
                m_viewport.setCameraSettings(settings);
            });
    } else {
        m_ui.numberFloat("toolbar.viewOptions.far")
            .parent("toolbar.viewOptions.farRow")
            .styleClass("plain")
            .value(cameraSettings.farPlane)
            .range(0.001f, 1000000000.0f)
            .precision(1)
            .sensitivity(10.0f)
            .grow(1.0f)
            .height(26.0f)
            .onChanged([this](const float value) {
                EditorViewportCameraSettings settings = m_viewport.cameraSettings();
                settings.farPlane = value;
                m_viewport.setCameraSettings(settings);
            });
    }

    m_ui.checkbox("toolbar.viewOptions.infiniteFar")
        .parent("toolbar.viewOptions")
        .label("Infinite Far Plane")
        .labelPlacement(UILabelPlacement::Right)
        .checked(cameraSettings.infiniteFarPlane)
        .height(28.0f)
        .onCheckedChanged([this](const bool checked) {
            EditorViewportCameraSettings settings = m_viewport.cameraSettings();
            settings.infiniteFarPlane = checked;
            m_viewport.setCameraSettings(settings);
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
        .width(84.0f)
        .height(24.0f)
        .onClick([this, &resources]() {
            refreshAssets(resources);
            spdlog::info("Editor UI: refreshed asset list.");
        });

    m_ui.button("assets.loadSprite")
        .parent("assets.header")
        .styleClass("toolbar")
        .text("Load Sprite")
        .textStyle("toolbar-toggle")
        .width(104.0f)
        .height(24.0f)
        .onClick([this, &resources]() {
            loadSpriteAsset(resources);
        });

    m_ui.button("assets.loadModel")
        .parent("assets.header")
        .styleClass("toolbar")
        .text("Load Model")
        .textStyle("toolbar-toggle")
        .width(104.0f)
        .height(24.0f)
        .onClick([this, &resources]() {
            loadModelAsset(resources);
        });

    const std::size_t visibleTextureCount = m_textureAssets.size();
    const std::size_t visibleModelCount = m_modelAssets.size();
    const std::size_t visibleAssetCount = visibleTextureCount + visibleModelCount;
    m_ui.panel("assets.list")
        .parent("console")
        .drawBackground(false)
        .grid(assetGridColumnsForWidth(width))
        .scrollable(true)
        .height(std::max(28.0f, m_consoleHeight - 62.0f))
        .padding(UIEdgeInsets::all(0.0f))
        .gap(6.0f);

    if (visibleAssetCount == 0U) {
        m_ui.label("assets.empty")
            .parent("assets.list")
            .text("No sprite/model assets found")
            .textStyle("muted")
            .height(22.0f);
    }

    for (std::size_t index = 0; index < visibleTextureCount; ++index) {
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
            .height(82.0f)
            .selected(selected)
            .tooltip(m_assetContextMenuOpen ? "" : asset.displayPath);
    }

    for (std::size_t index = 0; index < visibleModelCount; ++index) {
        const std::string id = "assets.model." + std::to_string(index);
        m_ui.button(id)
            .parent("assets.list")
            .styleClass("hierarchy-row")
            .text("")
            .textStyle("hierarchy-row")
            .height(82.0f)
            .tooltip(m_assetContextMenuOpen ? "" : m_modelAssets[index].displayPath)
            .onClick([this, index, &resources]() {
                loadModelAssetAt(index, resources);
            });
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

    const auto objects = m_scene.objects();

    if (objects.empty()) {
        m_ui.label("hierarchy.empty")
            .parent("hierarchy")
            .text("No scene objects")
            .textStyle("muted")
            .height(22.0f);
    }

    for (std::size_t index = 0; index < objects.size(); ++index) {
        const SceneObjectId objectId = objects[index].id;
        const std::string id = "hierarchy.row." + std::to_string(index);

        m_ui.button(id)
            .parent("hierarchy")
            .styleClass("hierarchy-row")
            .text(objects[index].name)
            .textStyle("hierarchy-row")
            .height(22.0f)
            .selected(m_selection.selectedSceneObjectId() == objectId)
            .onClick([this, objectId]() {
                m_selection.select(objectId);
                spdlog::info("Editor UI: Scene object {} selected from hierarchy.", objectId);
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
            .onTextChanged([selectedObject](const std::string_view value) {
                selectedObject->name = std::string(value);
            });

        m_ui.label("sceneObject.transform-label")
            .parent("inspector")
            .text("Transform")
            .textStyle("control-label")
            .height(18.0f);

        m_ui.panel("sceneObject.transform-group")
            .parent("inspector")
            .drawBackground(false)
            .vertical()
            .height(92.0f)
            .padding(UIEdgeInsets::all(0.0f))
            .gap(4.0f);

        m_ui.panel("sceneObject.location-row")
            .parent("sceneObject.transform-group")
            .drawBackground(false)
            .horizontal()
            .height(28.0f)
            .padding(UIEdgeInsets::all(0.0f))
            .gap(6.0f);

        m_ui.button("sceneObject.location-label")
            .parent("sceneObject.location-row")
            .styleClass("toolbar")
            .text("Location")
            .textStyle("toolbar-toggle")
            .width(88.0f)
            .height(28.0f);

        m_ui.panel("sceneObject.position-grid")
            .parent("sceneObject.location-row")
            .drawBackground(false)
            .grid(3)
            .grow(1.0f)
            .height(28.0f)
            .padding(UIEdgeInsets::all(0.0f))
            .gap(4.0f);

        m_ui.numberFloat("sceneObject.position.x")
            .parent("sceneObject.position-grid")
            .styleClass("plain")
            .value(selectedObject->transform.position.x)
            .range(-10000.0f, 10000.0f)
            .precision(2)
            .sensitivity(0.1f)
            .onChanged([selectedObject](const float value) {
                selectedObject->transform.position.x = value;
            });

        m_ui.numberFloat("sceneObject.position.y")
            .parent("sceneObject.position-grid")
            .styleClass("plain")
            .value(selectedObject->transform.position.y)
            .range(-10000.0f, 10000.0f)
            .precision(2)
            .sensitivity(0.1f)
            .onChanged([selectedObject](const float value) {
                selectedObject->transform.position.y = value;
            });

        m_ui.numberFloat("sceneObject.position.z")
            .parent("sceneObject.position-grid")
            .styleClass("plain")
            .value(selectedObject->transform.position.z)
            .range(-10000.0f, 10000.0f)
            .precision(2)
            .sensitivity(0.1f)
            .onChanged([selectedObject](const float value) {
                selectedObject->transform.position.z = value;
            });

        m_ui.panel("sceneObject.rotation-row")
            .parent("sceneObject.transform-group")
            .drawBackground(false)
            .horizontal()
            .height(28.0f)
            .padding(UIEdgeInsets::all(0.0f))
            .gap(6.0f);

        m_ui.button("sceneObject.rotation-label")
            .parent("sceneObject.rotation-row")
            .styleClass("toolbar")
            .text("Rotation")
            .textStyle("toolbar-toggle")
            .width(88.0f)
            .height(28.0f);

        m_ui.panel("sceneObject.rotation-grid")
            .parent("sceneObject.rotation-row")
            .drawBackground(false)
            .grid(3)
            .grow(1.0f)
            .height(28.0f)
            .padding(UIEdgeInsets::all(0.0f))
            .gap(4.0f);

        m_ui.numberFloat("sceneObject.rotation.x")
            .parent("sceneObject.rotation-grid")
            .styleClass("plain")
            .value(radiansToDegrees(selectedObject->transform.rotationRadians.x))
            .range(-360.0f, 360.0f)
            .precision(1)
            .sensitivity(0.5f)
            .onChanged([selectedObject](const float value) {
                selectedObject->transform.rotationRadians.x = degreesToRadians(value);
            });

        m_ui.numberFloat("sceneObject.rotation.y")
            .parent("sceneObject.rotation-grid")
            .styleClass("plain")
            .value(radiansToDegrees(selectedObject->transform.rotationRadians.y))
            .range(-360.0f, 360.0f)
            .precision(1)
            .sensitivity(0.5f)
            .onChanged([selectedObject](const float value) {
                selectedObject->transform.rotationRadians.y = degreesToRadians(value);
            });

        m_ui.numberFloat("sceneObject.rotation.z")
            .parent("sceneObject.rotation-grid")
            .styleClass("plain")
            .value(radiansToDegrees(selectedObject->transform.rotationRadians.z))
            .range(-360.0f, 360.0f)
            .precision(1)
            .sensitivity(0.5f)
            .onChanged([selectedObject](const float value) {
                selectedObject->transform.rotationRadians.z = degreesToRadians(value);
            });

        m_ui.panel("sceneObject.scale-row")
            .parent("sceneObject.transform-group")
            .drawBackground(false)
            .horizontal()
            .height(28.0f)
            .padding(UIEdgeInsets::all(0.0f))
            .gap(6.0f);

        m_ui.button("sceneObject.scale-label")
            .parent("sceneObject.scale-row")
            .styleClass("toolbar")
            .text("Scale")
            .textStyle("toolbar-toggle")
            .width(88.0f)
            .height(28.0f);

        m_ui.panel("sceneObject.scale-grid")
            .parent("sceneObject.scale-row")
            .drawBackground(false)
            .grid(3)
            .grow(1.0f)
            .height(28.0f)
            .padding(UIEdgeInsets::all(0.0f))
            .gap(4.0f);

        m_ui.numberFloat("sceneObject.scale.x")
            .parent("sceneObject.scale-grid")
            .styleClass("plain")
            .value(selectedObject->transform.scale.x)
            .range(0.01f, 100.0f)
            .precision(2)
            .sensitivity(0.05f)
            .onChanged([selectedObject](const float value) {
                selectedObject->transform.scale.x = value;
            });

        m_ui.numberFloat("sceneObject.scale.y")
            .parent("sceneObject.scale-grid")
            .styleClass("plain")
            .value(selectedObject->transform.scale.y)
            .range(0.01f, 100.0f)
            .precision(2)
            .sensitivity(0.05f)
            .onChanged([selectedObject](const float value) {
                selectedObject->transform.scale.y = value;
            });

        m_ui.numberFloat("sceneObject.scale.z")
            .parent("sceneObject.scale-grid")
            .styleClass("plain")
            .value(selectedObject->transform.scale.z)
            .range(0.01f, 100.0f)
            .precision(2)
            .sensitivity(0.05f)
            .onChanged([selectedObject](const float value) {
                selectedObject->transform.scale.z = value;
            });

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
                .onTextChanged([sprite](const std::string_view value) {
                    sprite->texturePath = std::string(value);
                });

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
                .onChanged([sprite](const float value) {
                    sprite->size.x = value;
                });

            m_ui.numberFloat("sceneObject.size.y")
                .parent("sceneObject.size-grid")
                .styleClass("plain")
                .value(sprite->size.y)
                .range(1.0f, 10000.0f)
                .precision(2)
                .sensitivity(0.5f)
                .onChanged([sprite](const float value) {
                    sprite->size.y = value;
                });

            m_ui.colorPicker("sceneObject.tint")
                .parent("inspector")
                .label("Tint")
                .labelStyle("control-label")
                .color(sprite->tint)
                .onColorChanged([sprite](const glm::vec4& color) {
                    sprite->tint = color;
                });

            m_ui.dropdown("sceneObject.blendMode")
                .parent("inspector")
                .styleClass("inspector")
                .label("Blend")
                .labelStyle("control-label")
                .text(blendModeName(sprite->renderState.blendMode))
                .popupSize({180.0f, 112.0f})
                .padding(UIEdgeInsets::all(8.0f))
                .gap(4.0f);

            const auto addBlendOption = [this, sprite](const std::string_view id, const WorldBlendMode mode) {
                m_ui.button(id)
                    .parent("sceneObject.blendMode")
                    .styleClass("hierarchy-row")
                    .text(blendModeName(mode))
                    .textStyle("hierarchy-row")
                    .height(28.0f)
                    .selected(sprite->renderState.blendMode == mode)
                    .onClick([this, sprite, mode]() {
                        sprite->renderState.blendMode = mode;
                        m_ui.closeDropdown("sceneObject.blendMode");
                    });
            };

            addBlendOption("sceneObject.blendMode.opaque", WorldBlendMode::Opaque);
            addBlendOption("sceneObject.blendMode.alpha", WorldBlendMode::Alpha);
            addBlendOption("sceneObject.blendMode.additive", WorldBlendMode::Additive);

            m_ui.dropdown("sceneObject.samplerMode")
                .parent("inspector")
                .styleClass("inspector")
                .label("Sampler")
                .labelStyle("control-label")
                .text(samplerModeName(sprite->renderState.samplerMode))
                .popupSize({180.0f, 80.0f})
                .padding(UIEdgeInsets::all(8.0f))
                .gap(4.0f);

            const auto addSamplerOption = [this, sprite](const std::string_view id, const WorldSamplerMode mode) {
                m_ui.button(id)
                    .parent("sceneObject.samplerMode")
                    .styleClass("hierarchy-row")
                    .text(samplerModeName(mode))
                    .textStyle("hierarchy-row")
                    .height(28.0f)
                    .selected(sprite->renderState.samplerMode == mode)
                    .onClick([this, sprite, mode]() {
                        sprite->renderState.samplerMode = mode;
                        m_ui.closeDropdown("sceneObject.samplerMode");
                    });
            };

            addSamplerOption("sceneObject.samplerMode.nearest", WorldSamplerMode::Nearest);
            addSamplerOption("sceneObject.samplerMode.linear", WorldSamplerMode::Linear);
        }
    } else {
        m_ui.label("inspector.no-selection")
            .parent("inspector")
            .text("No scene object selected")
            .textStyle("muted")
            .height(22.0f);
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

    if (m_hierarchyVisible) {
        const UIInteraction splitter = m_context.interact("splitter.hierarchy", m_hierarchySplitterBounds.position, m_hierarchySplitterBounds.size);
        if (splitter.hovered || splitter.held) {
            m_context.requestCursor(UICursor::ResizeHorizontal);
        }
        if (splitter.held) {
            m_hierarchyWidth = std::clamp(m_hierarchyWidth + delta.x, 150.0f, 320.0f);
            changed = true;
        }
    }
    if (m_inspectorVisible) {
        const UIInteraction splitter = m_context.interact("splitter.inspector", m_inspectorSplitterBounds.position, m_inspectorSplitterBounds.size);
        if (splitter.hovered || splitter.held) {
            m_context.requestCursor(UICursor::ResizeHorizontal);
        }
        if (splitter.held) {
            m_inspectorWidth = std::clamp(m_inspectorWidth - delta.x, 190.0f, 380.0f);
            changed = true;
        }
    }
    if (m_consoleVisible && m_context.interact("splitter.console", m_consoleSplitterBounds.position, m_consoleSplitterBounds.size).held) {
        m_consoleHeight = std::clamp(m_consoleHeight - delta.y, 116.0f, 360.0f);
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

    const glm::vec2 mouse = input.mousePosition();
    for (std::size_t index = 0; index < m_textureAssets.size(); ++index) {
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
    UITextStyle metadataText = mutedText;
    metadataText.scale = std::min(metadataText.scale, labelText.scale);
    const std::size_t visibleTextureCount = m_textureAssets.size();
    const std::size_t visibleModelCount = m_modelAssets.size();
    UIFrame frame{m_context, renderer2D, textRenderer, m_style};
    UICompositeRenderScope previewRenderScope{frame};

    const UIRect listBounds = m_ui.bounds("assets.list");
    if (listBounds.size.x <= 0.0f || listBounds.size.y <= 0.0f) {
        return;
    }

    struct TexturePreviewItem {
        const TextureAssetInfo* asset{nullptr};
        UIRect bounds{};
        glm::vec2 previewPosition{0.0f};
        glm::vec2 previewSize{48.0f};
    };

    struct ModelPreviewItem {
        const ModelAssetInfo* asset{nullptr};
        UIRect bounds{};
        glm::vec2 previewPosition{0.0f};
        glm::vec2 previewSize{48.0f};
    };

    std::vector<TexturePreviewItem> textureItems;
    textureItems.reserve(visibleTextureCount);

    for (std::size_t index = 0; index < visibleTextureCount; ++index) {
        const TextureAssetInfo& asset = m_textureAssets[index];
        const std::string id = "assets.texture." + std::to_string(index);
        const UIRect bounds = m_ui.bounds(id);
        if (bounds.size.x <= 0.0f || bounds.size.y <= 0.0f) {
            continue;
        }

        constexpr float thumbnailSize = 54.0f;
        textureItems.push_back({
            &asset,
            bounds,
            bounds.position + glm::vec2{10.0f, 14.0f},
            {thumbnailSize, thumbnailSize},
        });
    }

    std::vector<ModelPreviewItem> modelItems;
    modelItems.reserve(visibleModelCount);

    for (std::size_t index = 0; index < visibleModelCount; ++index) {
        const ModelAssetInfo& asset = m_modelAssets[index];
        const std::string id = "assets.model." + std::to_string(index);
        const UIRect bounds = m_ui.bounds(id);
        if (bounds.size.x <= 0.0f || bounds.size.y <= 0.0f) {
            continue;
        }

        constexpr float thumbnailSize = 54.0f;
        modelItems.push_back({
            &asset,
            bounds,
            bounds.position + glm::vec2{10.0f, 14.0f},
            {thumbnailSize, thumbnailSize},
        });
    }

    renderer2D.pushClipRect({listBounds.position, listBounds.size});
    textRenderer.pushClipRect({listBounds.position, listBounds.size});

    for (const TexturePreviewItem& item : textureItems) {
        renderer2D.drawSdfRect(item.previewPosition, item.previewSize, 3.0f, m_style.field.fill, m_style.field.border, 1.0f);
    }

    for (const TexturePreviewItem& item : textureItems) {
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

    for (const TexturePreviewItem& item : textureItems) {
        if (item.asset->handle) {
            renderer2D.drawRect(item.previewPosition, item.previewSize, m_style.field.border, 1.0f);
        }
    }

    for (const TexturePreviewItem& item : textureItems) {
        const TextureAssetInfo& asset = *item.asset;
        const std::string label = assetTileLabel(asset.displayPath);

        std::string sizeText = asset.loaded ? "Loaded" : "Missing";
        if (asset.handle) {
            if (const TextureResource* texture = resources.tryTexture(asset.handle)) {
                sizeText = texture->fallback == TextureFallbackKind::Missing
                    ? "Missing"
                    : std::to_string(texture->pixels.width) + " x " + std::to_string(texture->pixels.height);
            }
        }

        drawWrappedTextClipped(
            textRenderer,
            label,
            {
                item.bounds.position + glm::vec2{74.0f, 10.0f},
                {std::max(0.0f, item.bounds.size.x - 84.0f), 38.0f},
            },
            labelText);
        drawWrappedTextClipped(
            textRenderer,
            sizeText,
            {
                item.bounds.position + glm::vec2{74.0f, 56.0f},
                {std::max(0.0f, item.bounds.size.x - 84.0f), 20.0f},
            },
            metadataText,
            TextAlignment::Left,
            false);
    }

    for (const ModelPreviewItem& item : modelItems) {
        renderer2D.drawSdfRect(item.previewPosition, item.previewSize, 3.0f, m_style.field.fill, m_style.field.border, 1.0f);

        const glm::vec4 modelFill{0.20f, 0.30f, 0.36f, 1.0f};
        const glm::vec4 modelAccent{0.45f, 0.72f, 0.78f, 1.0f};
        const glm::vec2 innerSize = item.previewSize * glm::vec2{0.52f, 0.46f};
        const glm::vec2 innerPosition = item.previewPosition + (item.previewSize - innerSize) * 0.5f + glm::vec2{-2.0f, 2.0f};
        renderer2D.drawQuad(innerPosition + glm::vec2{5.0f, -5.0f}, innerSize, glm::vec4{modelFill.r, modelFill.g, modelFill.b, 0.45f});
        renderer2D.drawRect(innerPosition + glm::vec2{5.0f, -5.0f}, innerSize, modelAccent, 1.0f);
        renderer2D.drawQuad(innerPosition, innerSize, modelFill);
        renderer2D.drawRect(innerPosition, innerSize, modelAccent, 1.0f);
        renderer2D.drawQuad(
            item.previewPosition + glm::vec2{item.previewSize.x * 0.18f, item.previewSize.y * 0.78f},
            {item.previewSize.x * 0.64f, 3.0f},
            glm::vec4{modelAccent.r, modelAccent.g, modelAccent.b, 0.55f});

        textRenderer.drawText("3D", item.previewPosition + glm::vec2{item.previewSize.x * 0.30f, item.previewSize.y * 0.33f}, modelAccent, labelText.font, labelText.scale);
    }

    for (const ModelPreviewItem& item : modelItems) {
        const ModelAssetInfo& asset = *item.asset;
        const std::string label = assetTileLabel(asset.displayPath);

        std::string summary = asset.loaded ? "Loaded model" : "Not loaded";
        std::string bounds = "Click to load";
        if (asset.handle) {
            if (const ModelResource* model = resources.tryModel(asset.handle)) {
                summary = std::to_string(model->meshes.size()) + " mesh";
                if (model->meshes.size() != 1U) {
                    summary += "es";
                }
                summary += ", " + std::to_string(model->materials.size()) + " material";
                if (model->materials.size() != 1U) {
                    summary += "s";
                }
                bounds = modelBoundsText(model->bounds);
            }
        }

        drawWrappedTextClipped(
            textRenderer,
            label,
            {
                item.bounds.position + glm::vec2{74.0f, 8.0f},
                {std::max(0.0f, item.bounds.size.x - 84.0f), 34.0f},
            },
            labelText);
        drawWrappedTextClipped(
            textRenderer,
            summary,
            {
                item.bounds.position + glm::vec2{74.0f, 44.0f},
                {std::max(0.0f, item.bounds.size.x - 84.0f), 20.0f},
            },
            metadataText,
            TextAlignment::Left,
            false);
        drawWrappedTextClipped(
            textRenderer,
            bounds,
            {
                item.bounds.position + glm::vec2{74.0f, 61.0f},
                {std::max(0.0f, item.bounds.size.x - 84.0f), 20.0f},
            },
            metadataText,
            TextAlignment::Left,
            false);
    }

    textRenderer.popClipRect();
    renderer2D.popClipRect();
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
    refreshAssets(resources);

    spdlog::info("Editor UI: added scene sprite from asset '{}'.", displayPath);
}

void EditorUI::loadSpriteAsset(ResourceManager& resources)
{
    const std::optional<std::filesystem::path> selectedPath = FileDialog::openFile({
        "Load Sprite Asset",
        NIKREON_ASSET_DIR,
        {
            {"Sprite Images", {"*.png", "*.jpg", "*.jpeg"}},
            {"All Files", {"*.*"}},
        },
    });

    if (!selectedPath) {
        return;
    }

    const std::optional<std::filesystem::path> importedPath = importAssetFile(*selectedPath, "sprites");
    if (!importedPath) {
        return;
    }

    const TextureHandle handle = resources.loadTexture(*importedPath);
    TextureAssetInfo imported;
    imported.path = *importedPath;
    imported.displayPath = assetDisplayPath(*importedPath);
    imported.loaded = handle && handle != resources.missingTexture();
    imported.handle = handle;

    const std::string normalizedPath = resources.normalizePath(imported.path);
    const auto found = std::find_if(m_textureAssets.begin(), m_textureAssets.end(), [&resources, &normalizedPath](const TextureAssetInfo& asset) {
        return resources.normalizePath(asset.path) == normalizedPath;
    });

    if (found == m_textureAssets.end()) {
        m_textureAssets.insert(m_textureAssets.begin(), std::move(imported));
    } else {
        *found = std::move(imported);
    }

    m_assetsDirty = false;
    spdlog::info("Editor UI: imported sprite asset '{}'.", normalizedPath);
}

void EditorUI::loadModelAsset(ResourceManager& resources)
{
    const std::optional<std::filesystem::path> selectedPath = FileDialog::openFile({
        "Load Model Asset",
        NIKREON_ASSET_DIR,
        {
            {"glTF Models", {"*.glb", "*.gltf"}},
            {"All Files", {"*.*"}},
        },
    });

    if (!selectedPath) {
        return;
    }

    const std::optional<std::filesystem::path> importedPath = importAssetFile(*selectedPath, "models");
    if (!importedPath) {
        return;
    }

    const ModelHandle handle = resources.loadModel(*importedPath);
    ModelAssetInfo imported;
    imported.path = *importedPath;
    imported.displayPath = assetDisplayPath(*importedPath);
    imported.loaded = static_cast<bool>(handle);
    imported.handle = handle;

    const std::string normalizedPath = resources.normalizePath(imported.path);
    const auto found = std::find_if(m_modelAssets.begin(), m_modelAssets.end(), [&resources, &normalizedPath](const ModelAssetInfo& asset) {
        return resources.normalizePath(asset.path) == normalizedPath;
    });

    if (found == m_modelAssets.end()) {
        m_modelAssets.insert(m_modelAssets.begin(), std::move(imported));
    } else {
        *found = std::move(imported);
    }

    m_assetsDirty = false;
    spdlog::info("Editor UI: imported model asset '{}'.", normalizedPath);
}

void EditorUI::loadModelAssetAt(const std::size_t index, ResourceManager& resources)
{
    if (index >= m_modelAssets.size()) {
        return;
    }

    ModelAssetInfo& asset = m_modelAssets[index];
    const ModelHandle handle = resources.loadModel(asset.path);
    asset.loaded = static_cast<bool>(handle);
    asset.handle = handle;

    if (handle) {
        spdlog::info("Editor UI: loaded model asset '{}'.", asset.displayPath);
    } else {
        spdlog::warn("Editor UI: model asset '{}' could not be loaded.", asset.displayPath);
    }
}

void EditorUI::refreshAssets(ResourceManager& resources)
{
    m_textureAssets = resources.scanTextureAssets(NIKREON_ASSET_DIR);
    for (TextureAssetInfo& asset : m_textureAssets) {
        const TextureHandle handle = resources.loadTexture(asset.path);
        if (handle) {
            asset.loaded = handle != resources.missingTexture();
            asset.handle = handle;
        }
    }

    m_modelAssets = resources.scanModelAssets(NIKREON_ASSET_DIR);
    for (ModelAssetInfo& asset : m_modelAssets) {
        const ModelHandle handle = resources.loadModel(asset.path);
        if (handle) {
            asset.loaded = true;
            asset.handle = handle;
        }
    }

    m_assetsDirty = false;
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

bool EditorUI::keyboardInputCaptured() const
{
    return m_context.hasFocusedItem();
}

} // namespace Engine
