#include "Engine/Editor/EditorLayer.hpp"
#include "Engine/Renderer/Camera/Camera2D.hpp"
#include "Engine/Renderer/World2D/Renderer2DWorld.hpp"
#include "Engine/Core/Input.hpp"

#include <algorithm>

#include <GLFW/glfw3.h>

namespace Engine {

EditorLayer::EditorLayer()
    : m_ui(m_viewport, m_scene)
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

    updateViewportSelection(input, safeCamera.camera2D);

    renderer2DWorld.begin(safeCamera);
    m_scene2DSubmitter.submit(m_scene, renderer2DWorld, resources);
    drawSelectedSpriteOutline(renderer2DWorld);
    renderer2DWorld.end();
}

void EditorLayer::updateViewportSelection(const Input& input, const Camera2D& camera)
{
    const bool leftMousePressed = input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
    const bool leftClick = leftMousePressed && !m_leftMouseWasPressed;
    m_leftMouseWasPressed = leftMousePressed;

    if (!leftClick || !m_viewport.hovered() || m_viewport.cameraMode() != EditorCameraMode::Orthographic2D) {
        return;
    }

    const std::optional<glm::vec2>& localMouse = m_viewport.localMousePosition();
    if (!localMouse) {
        return;
    }

    m_ui.setSelectedSceneObject(pickSpriteAt(camera.screenToWorld(*localMouse)));
}

SceneObjectId EditorLayer::pickSpriteAt(const glm::vec2& worldPosition) const
{
    const std::span<const SceneObject> objects = m_scene.objects();
    for (auto iterator = objects.rbegin(); iterator != objects.rend(); ++iterator) {
        const SceneObject& object = *iterator;
        if (!object.sprite2D) {
            continue;
        }

        const Sprite2DComponent& sprite = *object.sprite2D;
        const glm::vec2 size = sprite.size * glm::vec2{object.transform.scale.x, object.transform.scale.y};
        const glm::vec2 minimum = glm::vec2{object.transform.position.x, object.transform.position.y} - size * sprite.origin;
        const glm::vec2 maximum = minimum + size;

        if (worldPosition.x >= minimum.x &&
            worldPosition.y >= minimum.y &&
            worldPosition.x <= maximum.x &&
            worldPosition.y <= maximum.y) {
            return object.id;
        }
    }

    return InvalidSceneObjectId;
}

void EditorLayer::drawSelectedSpriteOutline(Renderer2DWorld& renderer2DWorld) const
{
    const SceneObject* selected = m_scene.findObject(m_ui.selectedSceneObjectId());
    if (selected == nullptr || !selected->sprite2D) {
        return;
    }

    const Sprite2DComponent& sprite = *selected->sprite2D;
    const glm::vec2 size = sprite.size * glm::vec2{selected->transform.scale.x, selected->transform.scale.y};
    const glm::vec2 minimum = glm::vec2{selected->transform.position.x, selected->transform.position.y} - size * sprite.origin;
    renderer2DWorld.drawDebugRect(minimum, size, {0.42f, 0.72f, 1.0f, 1.0f}, 2.0f);
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
