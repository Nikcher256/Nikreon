#pragma once

#include "Engine/Editor/EditorWorldDebugController.hpp"
#include "Engine/Editor/EditorViewport.hpp"
#include "Engine/UI/Layout.hpp"
#include "Engine/UI/UIBuilder.hpp"
#include "Engine/UI/UIContext.hpp"
#include "Engine/UI/UIStyle.hpp"

#include <cstddef>
#include <string>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Engine {

class Input;
class Renderer2D;
class TextRenderer;

class EditorUI {
public:
    explicit EditorUI(EditorViewport& viewport, EditorWorldDebugController& worldDebug);

    void update(float deltaTime);
    void render(Renderer2D& renderer2D, TextRenderer& textRenderer, const glm::uvec2& viewportSize, const Input& input);
    [[nodiscard]] const UIRect& viewportBounds() const;

private:
    enum class RunState {
        Stopped,
        Playing,
        Paused,
    };

    void declareUI(float width, float height);
    void syncBuilderBounds();
    [[nodiscard]] bool updatePanelSplitters();
    void renderPanelSplitters(Renderer2D& renderer2D);
    void drawViewportGrid(Renderer2D& renderer2D, const UIRect& bounds);
    void setViewportModeFromIndex(std::size_t index);
    void openSpriteFileDialog();

    EditorViewport& m_viewport;
    EditorWorldDebugController& m_worldDebug;
    UIStyle m_style;
    UIContext m_context;
    UIBuilder m_ui;

    struct EditorStyle {
        glm::vec4 viewportGrid{0.10f, 0.13f, 0.17f, 0.8f};
        glm::vec4 viewportBorder{0.25f, 0.32f, 0.42f, 1.0f};
        glm::vec4 viewportFocusedBorder{0.32f, 0.58f, 0.88f, 1.0f};
        float toolbarHeightMin{42.0f};
        float toolbarHeightMax{52.0f};
    } m_editorStyle;

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
    glm::vec2 m_renderSize{1.0f, 1.0f};
    int m_selectedHierarchyRow{0};
    RunState m_runState{RunState::Stopped};
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
    std::string m_objectName{"Directional Light"};
    std::string m_spritePath{"assets/sprites/test.png"};
};

} // namespace Engine
