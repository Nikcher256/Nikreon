#include "Engine/Editor/EditorLayer.hpp"
#include "Engine/Renderer/Camera/Camera2D.hpp"
#include "Engine/Renderer/World2D/Renderer2DWorld.hpp"
#include "Engine/Core/Input.hpp"

#include <GLFW/glfw3.h>

namespace Engine {

EditorLayer::EditorLayer()
    : m_worldDebug("assets/sprites/test.png")
    , m_ui(m_viewport, m_worldDebug)
{
}

// Updates editor-owned state before rendering.
void EditorLayer::onUpdate(const float deltaTime, const Input& input)
{
    m_ui.update(deltaTime);
    updateEditorCamera(deltaTime, input);
}

const EditorViewport& EditorLayer::viewport() const
{
    return m_viewport;
}

// Draws the editor layer after the renderer has started a frame.
void EditorLayer::onRender(Renderer2D& renderer2D, Renderer2DWorld& renderer2DWorld, TextRenderer& textRenderer, const glm::uvec2& viewportSize, const Input& input)
{
    m_ui.render(renderer2D, textRenderer, viewportSize, input);
    
    Renderer2DWorldCamera worldCamera;
    worldCamera.mode = m_viewport.cameraMode() == EditorCameraMode::Perspective3D
        ? Renderer2DWorldCameraMode::Perspective3D
        : Renderer2DWorldCameraMode::Orthographic2D;

    worldCamera.camera2D.position = {
        m_viewport.editorCameraPosition().x,
        m_viewport.editorCameraPosition().y,
    };
    worldCamera.camera2D.viewportSize = m_ui.viewportBounds().size;
    worldCamera.camera2D.zoom = m_viewport.editorCameraZoom();

    const glm::vec2 viewportBoundsSize = glm::max(m_ui.viewportBounds().size, glm::vec2{1.0f, 1.0f});
    worldCamera.camera3D = m_viewport.worldCamera3D(viewportBoundsSize.x / viewportBoundsSize.y);

    m_worldDebug.submit(renderer2DWorld, worldCamera);
}

void EditorLayer::updateEditorCamera(const float deltaTime, const Input& input)
{
    const glm::vec2 mousePosition = input.mousePosition();
    const glm::vec2 mouseDelta = mousePosition - m_previousMousePosition;
    m_previousMousePosition = mousePosition;

    const bool perspective3D = m_viewport.cameraMode() == EditorCameraMode::Perspective3D;
    const bool controlDown =
        input.isKeyPressed(GLFW_KEY_LEFT_CONTROL) ||
        input.isKeyPressed(GLFW_KEY_RIGHT_CONTROL);

    const bool panning =
        m_viewport.focused() &&
        m_viewport.hovered() &&
        !controlDown &&
        input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_MIDDLE);

    const bool rotating3D =
        perspective3D &&
        controlDown &&
        m_viewport.focused() &&
        m_viewport.hovered() &&
        input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_MIDDLE);

    const auto axis = [&input](const int positive, const int negative) {
        return static_cast<float>(input.isKeyPressed(positive)) -
               static_cast<float>(input.isKeyPressed(negative));
    };

    m_viewport.updateEditorCamera(deltaTime, {
        .panDelta = panning ? mouseDelta : glm::vec2{0.0f, 0.0f},
        .lookDelta = rotating3D ? mouseDelta : glm::vec2{0.0f, 0.0f},
        .zoomDelta = m_viewport.hovered() ? input.scrollDelta().y : 0.0f,
        .moveRight = axis(GLFW_KEY_D, GLFW_KEY_A),
        .moveUp = axis(GLFW_KEY_E, GLFW_KEY_Q),
        .moveForward = axis(GLFW_KEY_W, GLFW_KEY_S),
        .perspective3D = perspective3D,
    });
}

} // namespace Engine
