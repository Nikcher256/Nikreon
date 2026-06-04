#include "Engine/Editor/EditorLayer.hpp"
#include "Engine/Renderer/Camera2D.hpp"
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
    
    Camera2D worldCamera;
    worldCamera.position = {
        m_viewport.editorCameraPosition().x,
        m_viewport.editorCameraPosition().y,
    };
    worldCamera.viewportSize = m_ui.viewportBounds().size;
    worldCamera.zoom = m_viewport.editorCameraZoom();

    m_worldDebug.submit(renderer2DWorld, worldCamera);
}

void EditorLayer::updateEditorCamera(const float deltaTime, const Input& input)
{
    const glm::vec2 mousePosition = input.mousePosition();
    const glm::vec2 mouseDelta = mousePosition - m_previousMousePosition;
    m_previousMousePosition = mousePosition;

    const bool panning = 
        m_viewport.focused() &&
        m_viewport.hovered() &&
        input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_MIDDLE);

    const auto axis = [&input](const int positive, const int negative) {
        return static_cast<float>(input.isKeyPressed(positive)) -
               static_cast<float>(input.isKeyPressed(negative));
    };

    m_viewport.updateEditorCamera(deltaTime, {
        .panDelta = panning ? mouseDelta: glm::vec2{0.0f, 0.0f},
        .zoomDelta = m_viewport.hovered() ? input.scrollDelta().y : 0.0f,
        .moveRight = axis(GLFW_KEY_D, GLFW_KEY_A),
        .moveUp = axis(GLFW_KEY_E, GLFW_KEY_Q),
        .moveForward = axis(GLFW_KEY_W, GLFW_KEY_S),
    });
}

} // namespace Engine
