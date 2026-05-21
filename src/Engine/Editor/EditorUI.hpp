#pragma once

#include "Engine/Editor/UIContext.hpp"

#include <glm/vec2.hpp>

namespace Engine {

class Input;
class Renderer2D;

class EditorUI {
public:
    void render(Renderer2D& renderer2D, const glm::uvec2& viewportSize, const Input& input);

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

    void drawPanel(Renderer2D& renderer2D, const glm::vec2& position, const glm::vec2& size);
    void drawButton(Renderer2D& renderer2D, const glm::vec2& position, const glm::vec2& size, const UIInteraction& state, bool selected = false);
    void drawToolbarIcon(Renderer2D& renderer2D, const glm::vec2& position, const glm::vec2& size, ToolbarIcon icon, bool selected);
    void drawCheckbox(Renderer2D& renderer2D, const glm::vec2& position, const glm::vec2& size, bool checked, const UIInteraction& state);
    void drawSlider(Renderer2D& renderer2D, const glm::vec2& position, const glm::vec2& size, float value, float minValue, float maxValue, const UIInteraction& state);
    void drawViewportGrid(Renderer2D& renderer2D, const glm::vec2& position, const glm::vec2& size);

    UIContext m_context;
    int m_selectedHierarchyRow{0};
    RunState m_runState{RunState::Stopped};
    bool m_showGrid{true};
    bool m_viewportFocused{false};
    float m_previewExposure{0.65f};
    float m_lastLoggedExposure{0.65f};
};

} // namespace Engine
