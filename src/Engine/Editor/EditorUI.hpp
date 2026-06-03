#pragma once

#include "Engine/Editor/EditorViewport.hpp"
#include "Engine/UI/Button.hpp"
#include "Engine/UI/Checkbox.hpp"
#include "Engine/UI/ColorPicker.hpp"
#include "Engine/UI/FilePathInput.hpp"
#include "Engine/UI/Layout.hpp"
#include "Engine/UI/NumberInput.hpp"
#include "Engine/UI/ScrollContainer.hpp"
#include "Engine/UI/Slider.hpp"
#include "Engine/UI/TextInput.hpp"
#include "Engine/UI/UIContext.hpp"
#include "Engine/UI/UIStyle.hpp"

#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Engine {

class Input;
class Renderer2D;
class Renderer2DWorld;
class TextRenderer;

class EditorUI {
public:
    EditorUI();

    void render(Renderer2D& renderer2D, Renderer2DWorld& renderer2DWorld, TextRenderer& textRenderer, const glm::uvec2& viewportSize, const Input& input);
    void updateEditorCamera(float deltaTime, const Input& input);
    [[nodiscard]] const EditorViewport& viewport() const;

private:
    enum class RunState {
        Stopped,
        Playing,
        Paused,
    };

    enum class ToolbarIcon {
        Play,
        Pause,
        Stop,
    };

    void layoutWidgets(float width, float height);
    void updatePanelSplitters(float width, float height);
    void updateWidgets(TextRenderer& textRenderer);
    void renderWidgets(Renderer2D& renderer2D, TextRenderer& textRenderer);
    void renderPanelSplitters(Renderer2D& renderer2D);
    void renderLabels(TextRenderer& textRenderer);
    void submitWorldRendererTestContent(Renderer2DWorld& renderer2DWorld);
    void renderWorldRendererLabels(TextRenderer& textRenderer, const Renderer2DWorld& renderer2DWorld);
    void drawStyledText(
        TextRenderer& textRenderer,
        std::string_view text,
        const UIRect& bounds,
        std::string_view styleClass,
        std::string_view id = {});
    void drawPanel(Renderer2D& renderer2D, const UIRect& bounds);
    void drawToolbarIcon(Renderer2D& renderer2D, const glm::vec2& position, const glm::vec2& size, ToolbarIcon icon, bool selected);
    void drawViewportGrid(Renderer2D& renderer2D, const UIRect& bounds);

    UIStyle m_style;
    struct EditorStyle {
        glm::vec4 toolbarFill{0.10f, 0.11f, 0.14f, 1.0f};
        glm::vec4 viewportGrid{0.10f, 0.13f, 0.17f, 0.8f};
        glm::vec4 viewportBorder{0.25f, 0.32f, 0.42f, 1.0f};
        glm::vec4 viewportFocusedBorder{0.32f, 0.58f, 0.88f, 1.0f};
        float toolbarHeightMin{42.0f};
        float toolbarHeightMax{52.0f};
    } m_editorStyle;
    UIContext m_context;
    Button m_playButton;
    Button m_pauseButton;
    Button m_stopButton;
    Button m_toggleHierarchyButton;
    Button m_toggleInspectorButton;
    Button m_toggleConsoleButton;
    Button m_editModeButton;
    Button m_playModeButton;
    Button m_simulateModeButton;
    Button m_hudEditModeButton;
    Button m_addSpriteButton;
    Button m_addManySpritesButton;
    Button m_addTilemapButton;
    Button m_animateSpriteButton;
    Button m_toggleParallaxButton;
    Button m_toggleParticlesButton;
    Button m_toggleDebugShapesButton;
    Button m_clearWorldTestButton;
    Checkbox m_gridCheckbox;
    ColorPicker m_clearColorPicker;
    Slider m_exposureSlider;
    NumberInput m_lightIntensityInput;
    NumberInput m_positionXInput;
    NumberInput m_positionYInput;
    NumberInput m_positionZInput;
    TextInput m_objectNameInput;
    FilePathInput m_spritePathInput;
    ScrollContainer m_inspectorScroll;
    std::vector<Button> m_hierarchyRows;
    UIRect m_hierarchyBounds;
    UIRect m_inspectorBounds;
    UIRect m_consoleBounds;
    UIRect m_viewportBounds;
    UIRect m_hierarchySplitterBounds;
    UIRect m_inspectorSplitterBounds;
    UIRect m_consoleSplitterBounds;
    float m_toolbarHeight{48.0f};
    float m_hierarchyWidth{192.0f};
    float m_inspectorWidth{230.0f};
    float m_consoleHeight{130.0f};
    glm::vec2 m_previousMousePosition{0.0f, 0.0f};
    int m_selectedHierarchyRow{0};
    RunState m_runState{RunState::Stopped};
    EditorViewport m_viewport;
    bool m_showGrid{true};
    glm::vec4 m_viewportClearColor{0.055f, 0.085f, 0.14f, 1.0f};
    bool m_hierarchyVisible{true};
    bool m_inspectorVisible{true};
    bool m_consoleVisible{true};
    bool m_hierarchyCollapsed{false};
    bool m_inspectorCollapsed{false};
    bool m_consoleCollapsed{false};
    float m_previewExposure{0.65f};
    float m_lastLoggedExposure{0.65f};
    float m_lightIntensity{4.0f};
    glm::vec3 m_previewPosition{12.5f, -4.0f, 8.0f};
    std::string m_loadedSpritePath;
    std::size_t m_worldTestSpriteCount{0};
    bool m_worldTestSpriteLoaded{false};
    bool m_worldTestTilemap{false};
    bool m_worldTestAnimated{false};
    bool m_worldTestParallax{false};
    bool m_worldTestParticles{false};
    bool m_worldTestDebugShapes{false};
    float m_worldTestElapsed{0.0f};
};

} // namespace Engine
