#include "Engine/Editor/EditorViewport.hpp"

#include <algorithm>

#include <glm/ext/scalar_constants.hpp>

namespace Engine {

namespace {

float degreesToRadians(const float degrees)
{
    return degrees * glm::pi<float>() / 180.0f;
}

} // namespace

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

std::string_view editorViewOrientationName(const EditorViewOrientation orientation)
{
    switch (orientation) {
    case EditorViewOrientation::Perspective: return "Perspective";
    case EditorViewOrientation::Top: return "Top";
    case EditorViewOrientation::Bottom: return "Bottom";
    case EditorViewOrientation::Front: return "Front";
    case EditorViewOrientation::Back: return "Back";
    case EditorViewOrientation::Left: return "Left";
    case EditorViewOrientation::Right: return "Right";
    }

    return "Top";
}

bool editorViewOrientationIsPerspective(const EditorViewOrientation orientation)
{
    return orientation == EditorViewOrientation::Perspective;
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

void EditorViewport::setViewOrientation(const EditorViewOrientation orientation)
{
    m_viewOrientation = orientation;
}

void EditorViewport::setGridVisible(const bool visible)
{
    m_gridVisible = visible;
}

void EditorViewport::setCameraSettings(const EditorViewportCameraSettings& settings)
{
    m_cameraSettings.verticalFovDegrees = std::clamp(settings.verticalFovDegrees, 5.0f, 170.0f);
    m_cameraSettings.nearPlane = std::clamp(settings.nearPlane, 0.001f, 1000000.0f);
    m_cameraSettings.farPlane = std::max(settings.farPlane, m_cameraSettings.nearPlane + 0.001f);
    m_cameraSettings.infiniteFarPlane = settings.infiniteFarPlane;
}

Camera3D EditorViewport::worldCamera3D(const float aspectRatio) const
{
    Camera3D camera = m_editorCamera.camera3D(aspectRatio);
    camera.verticalFovRadians = degreesToRadians(m_cameraSettings.verticalFovDegrees);
    camera.nearPlane = m_cameraSettings.nearPlane;
    camera.farPlane = m_cameraSettings.farPlane;
    camera.infiniteFarPlane = m_cameraSettings.infiniteFarPlane;
    return camera;
}

const EditorViewportCameraSettings& EditorViewport::cameraSettings() const
{
    return m_cameraSettings;
}

EditorViewOrientation EditorViewport::viewOrientation() const
{
    return m_viewOrientation;
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
