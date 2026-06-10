#include "Engine/Editor/EditorViewport.hpp"

#include <algorithm>

namespace Engine {

std::string_view editorViewportModeName(const EditorViewportMode mode)
{
    switch (mode) {
    case EditorViewportMode::Edit: return "Edit";
    case EditorViewportMode::Play: return "Play";
    case EditorViewportMode::Simulate: return "Simulate";
    case EditorViewportMode::HudEdit: return "HudEdit";
    }

    return "Edit";
}

std::string_view editorCameraModeName(const EditorCameraMode mode)
{
    switch (mode) {
    case EditorCameraMode::Orthographic2D: return "2D Camera";
    case EditorCameraMode::Perspective3D: return "3D Perspective";
    }

    return "2D Camera";
}

void EditorViewport::updateInteraction(
    const EditorViewportPresentation& presentation,
    const glm::vec2& mousePosition,
    const bool leftMousePressed)
{
    m_presentation = presentation;
    const glm::vec2 localPosition = mousePosition - presentation.position;
    m_hovered =
        presentation.size.x > 0.0f &&
        presentation.size.y > 0.0f &&
        localPosition.x >= 0.0f &&
        localPosition.y >= 0.0f &&
        localPosition.x < presentation.size.x &&
        localPosition.y < presentation.size.y;

    if (m_hovered) {
        m_localMousePosition = localPosition;
        m_pickingCoordinates = glm::uvec2{
            static_cast<unsigned int>(localPosition.x),
            static_cast<unsigned int>(localPosition.y),
        };
    } else {
        m_localMousePosition.reset();
        m_pickingCoordinates.reset();
    }

    if (leftMousePressed && !m_leftMouseWasPressed) {
        m_focused = m_hovered;
    }
    m_leftMouseWasPressed = leftMousePressed;
}

void EditorViewport::updateEditorCamera(const float deltaTime, const EditorCameraInput& input)
{
    if (!m_focused || (m_mode != EditorViewportMode::Edit && m_mode != EditorViewportMode::Simulate)) {
        return;
    }

    m_editorCamera.update(deltaTime, input);
}

void EditorViewport::setMode(const EditorViewportMode mode)
{
    m_mode = mode;
}

const EditorViewportPresentation& EditorViewport::presentation() const
{
    return m_presentation;
}

EditorViewportMode EditorViewport::mode() const
{
    return m_mode;
}

void EditorViewport::setCameraMode(const EditorCameraMode mode)
{
    m_cameraMode = mode;
}

void EditorViewport::setGridVisible(const bool visible)
{
    m_gridVisible = visible;
}

Camera3D EditorViewport::worldCamera3D(const float aspectRatio) const
{
    return m_editorCamera.camera3D(aspectRatio);
}

EditorCameraMode EditorViewport::cameraMode() const
{
    return m_cameraMode;
}

bool EditorViewport::gridVisible() const
{
    return m_gridVisible;
}

bool EditorViewport::hovered() const
{
    return m_hovered;
}

bool EditorViewport::focused() const
{
    return m_focused;
}

const std::optional<glm::vec2>& EditorViewport::localMousePosition() const
{
    return m_localMousePosition;
}

const std::optional<glm::uvec2>& EditorViewport::pickingCoordinates() const
{
    return m_pickingCoordinates;
}

const glm::vec3& EditorViewport::editorCameraPosition() const
{
    return m_editorCamera.position();
}

float EditorViewport::editorCameraZoom() const
{
    return m_editorCamera.zoom();
}

} // namespace Engine
