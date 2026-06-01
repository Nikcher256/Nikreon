#pragma once

#include "Engine/UI/Button.hpp"
#include "Engine/UI/Checkbox.hpp"
#include "Engine/UI/Layout.hpp"
#include "Engine/UI/NumberInput.hpp"
#include "Engine/UI/ScrollContainer.hpp"
#include "Engine/UI/Slider.hpp"
#include "Engine/UI/TextInput.hpp"
#include "Engine/UI/UIContext.hpp"
#include "Engine/UI/UIStyle.hpp"

#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace Engine {

class Input;
class Renderer2D;
class TextRenderer;

class EditorUI {
public:
    EditorUI();

    void render(Renderer2D& renderer2D, TextRenderer& textRenderer, const glm::uvec2& viewportSize, const Input& input);

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
    void renderWidgets(Renderer2D& renderer2D);
    void renderPanelSplitters(Renderer2D& renderer2D);
    void renderLabels(TextRenderer& textRenderer);
    void drawStyledText(
        TextRenderer& textRenderer,
        std::string_view text,
        const UIRect& bounds,
        std::string_view styleClass,
        std::string_view id = {});
    void drawTextInputValue(TextRenderer& textRenderer, const TextInput& input, const UIBoxStyle& boxStyle);
    void drawPanel(Renderer2D& renderer2D, const UIRect& bounds);
    void drawToolbarIcon(Renderer2D& renderer2D, const glm::vec2& position, const glm::vec2& size, ToolbarIcon icon, bool selected);
    void drawViewportGrid(Renderer2D& renderer2D, const UIRect& bounds);

    UIStyle m_style;
    UIContext m_context;
    Button m_playButton;
    Button m_pauseButton;
    Button m_stopButton;
    Button m_toggleHierarchyButton;
    Button m_toggleInspectorButton;
    Button m_toggleConsoleButton;
    Checkbox m_gridCheckbox;
    Slider m_exposureSlider;
    NumberInput m_lightIntensityInput;
    NumberInput m_positionXInput;
    NumberInput m_positionYInput;
    NumberInput m_positionZInput;
    TextInput m_objectNameInput;
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
    bool m_showGrid{true};
    bool m_viewportFocused{false};
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
};

} // namespace Engine
