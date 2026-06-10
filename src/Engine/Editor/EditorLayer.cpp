#include "Engine/Editor/EditorLayer.hpp"
#include "Engine/Renderer/Camera/Camera2D.hpp"
#include "Engine/Renderer/DebugDraw/DebugRenderer.hpp"
#include "Engine/Renderer/World2D/Renderer2DWorld.hpp"
#include "Engine/Core/Input.hpp"

#include <algorithm>
#include <cmath>

#include <GLFW/glfw3.h>
#include <glm/geometric.hpp>

namespace Engine {

EditorLayer::EditorLayer()
    : m_ui(m_viewport, m_scene, m_selection)
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
void EditorLayer::onRender(
    Renderer2D& renderer2D,
    Renderer2DWorld& renderer2DWorld,
    DebugRenderer& debugRenderer,
    TextRenderer& textRenderer,
    ResourceManager& resources,
    const glm::uvec2& viewportSize,
    const Input& input)
{
    m_ui.render(renderer2D, textRenderer, resources, viewportSize, input);
    
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

    Renderer2DWorldCamera safeCamera = worldCamera;
    safeCamera.camera2D.viewportSize = glm::max(safeCamera.camera2D.viewportSize, glm::vec2{1.0f, 1.0f});
    safeCamera.camera3D.aspectRatio = std::max(safeCamera.camera3D.aspectRatio, 0.001f);

    m_selectionController.update(input, m_viewport, safeCamera.camera2D, m_scene, m_selection);

    renderer2DWorld.begin(safeCamera);
    m_overlayController.draw(debugRenderer, m_viewport, safeCamera);
    m_scene2DSubmitter.submit(m_scene, renderer2DWorld, resources);
    m_selectionController.drawOverlay(renderer2DWorld, debugRenderer, m_scene, safeCamera.camera2D, m_selection);
    renderer2DWorld.end();
}

void EditorLayer::updateEditorCamera(const float deltaTime, const Input& input)
{
    const bool perspective3D = m_viewport.cameraMode() == EditorCameraMode::Perspective3D;
    const bool altDown =
        input.isKeyPressed(GLFW_KEY_LEFT_ALT) ||
        input.isKeyPressed(GLFW_KEY_RIGHT_ALT);
    const bool shiftDown =
        input.isKeyPressed(GLFW_KEY_LEFT_SHIFT) ||
        input.isKeyPressed(GLFW_KEY_RIGHT_SHIFT);
    const bool leftMouse = input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
    const bool middleMouse = input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_MIDDLE);
    const bool rightMouse = input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
    const bool cameraGestureCandidate = perspective3D
        ? ((altDown && leftMouse) || middleMouse || rightMouse)
        : middleMouse;

    if (!cameraGestureCandidate) {
        m_cameraMouseGestureActive = false;
    } else if (!m_cameraMouseGestureActive && m_viewport.focused() && m_viewport.hovered()) {
        m_cameraMouseGestureActive = true;
    }

    glm::vec2 mousePosition = input.mousePosition();
    const glm::vec2 wrappedMousePosition = wrapCameraMouseIfNeeded(input, mousePosition, m_cameraMouseGestureActive);
    const bool wrappedMouse =
        wrappedMousePosition.x != mousePosition.x ||
        wrappedMousePosition.y != mousePosition.y;
    mousePosition = wrappedMousePosition;
    if (wrappedMouse) {
        m_previousMousePosition = mousePosition;
    }

    const glm::vec2 mouseDelta = mousePosition - m_previousMousePosition;
    m_previousMousePosition = mousePosition;

    const bool viewportActive = m_viewport.focused() && (m_viewport.hovered() || m_cameraMouseGestureActive);

    const bool unrealOrbiting = perspective3D && viewportActive && altDown && leftMouse;
    const bool blenderOrbiting = perspective3D && viewportActive && !altDown && !shiftDown && middleMouse;
    const bool looking3D = perspective3D && viewportActive && rightMouse && !altDown;
    const bool dollying3D = perspective3D && viewportActive && altDown && rightMouse;
    const bool tracking3D = perspective3D && viewportActive && middleMouse && (altDown || shiftDown);
    const bool panning2D = !perspective3D && viewportActive && middleMouse;
    const bool focusRequested =
        viewportActive &&
        std::find(input.pressedKeys().begin(), input.pressedKeys().end(), GLFW_KEY_F) != input.pressedKeys().end();

    CameraFocusTarget focusTarget = selectedCameraFocusTarget();

    const auto axis = [&input](const int positive, const int negative) {
        return static_cast<float>(input.isKeyPressed(positive)) -
               static_cast<float>(input.isKeyPressed(negative));
    };

    m_viewport.updateEditorCamera(deltaTime, {
        .panDelta = panning2D ? mouseDelta : glm::vec2{0.0f, 0.0f},
        .lookDelta = looking3D ? mouseDelta : glm::vec2{0.0f, 0.0f},
        .orbitDelta = (unrealOrbiting || blenderOrbiting) ? mouseDelta : glm::vec2{0.0f, 0.0f},
        .trackDelta = tracking3D ? mouseDelta : glm::vec2{0.0f, 0.0f},
        .zoomDelta = m_viewport.hovered() ? input.scrollDelta().y : 0.0f,
        .dollyDelta = dollying3D ? mouseDelta.y : 0.0f,
        .moveRight = looking3D ? axis(GLFW_KEY_D, GLFW_KEY_A) : 0.0f,
        .moveUp = looking3D ? axis(GLFW_KEY_E, GLFW_KEY_Q) : 0.0f,
        .moveForward = looking3D ? axis(GLFW_KEY_W, GLFW_KEY_S) : 0.0f,
        .speedScale = shiftDown ? 3.0f : 1.0f,
        .focusRequested = focusRequested && focusTarget.valid,
        .perspective3D = perspective3D,
        .focusPosition = focusTarget.position,
        .focusRadius = focusTarget.radius,
    });
}

EditorLayer::CameraFocusTarget EditorLayer::selectedCameraFocusTarget() const
{
    const SceneObject* object = m_scene.findObject(m_selection.selectedSceneObjectId());
    if (object == nullptr) {
        return {};
    }

    CameraFocusTarget target;
    target.valid = true;
    target.position = object->transform.position;
    target.radius = 64.0f;

    if (object->sprite2D) {
        const Sprite2DComponent& sprite = *object->sprite2D;
        const glm::vec2 size = sprite.size * glm::vec2{
            std::abs(object->transform.scale.x),
            std::abs(object->transform.scale.y),
        };
        target.radius = std::max(glm::length(size) * 0.5f, 8.0f);
    }

    return target;
}

glm::vec2 EditorLayer::wrapCameraMouseIfNeeded(
    const Input& input,
    const glm::vec2& mousePosition,
    const bool cameraMouseGestureActive)
{
    if (!cameraMouseGestureActive) {
        return mousePosition;
    }

    const EditorViewportPresentation& presentation = m_viewport.presentation();
    const glm::vec2 viewportPosition = presentation.position;
    const glm::vec2 viewportSize = presentation.size;
    if (viewportSize.x <= 2.0f || viewportSize.y <= 2.0f) {
        return mousePosition;
    }

    constexpr float WrapPadding = 2.0f;
    const float left = viewportPosition.x;
    const float right = viewportPosition.x + viewportSize.x;
    const float top = viewportPosition.y;
    const float bottom = viewportPosition.y + viewportSize.y;

    glm::vec2 wrappedMousePosition = mousePosition;
    if (wrappedMousePosition.x < left) {
        wrappedMousePosition.x = right - WrapPadding;
    } else if (wrappedMousePosition.x >= right) {
        wrappedMousePosition.x = left + WrapPadding;
    }

    if (wrappedMousePosition.y < top) {
        wrappedMousePosition.y = bottom - WrapPadding;
    } else if (wrappedMousePosition.y >= bottom) {
        wrappedMousePosition.y = top + WrapPadding;
    }

    if (wrappedMousePosition.x != mousePosition.x || wrappedMousePosition.y != mousePosition.y) {
        input.setMousePosition(wrappedMousePosition);
    }

    return wrappedMousePosition;
}

} // namespace Engine
