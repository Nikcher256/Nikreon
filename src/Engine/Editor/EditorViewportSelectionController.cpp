#include "Engine/Editor/EditorViewportSelectionController.hpp"

#include "Engine/Core/Input.hpp"
#include "Engine/Renderer/DebugDraw/DebugRenderer.hpp"
#include "Engine/Renderer/World2D/Renderer2DWorld.hpp"

#include <algorithm>
#include <span>

#include <GLFW/glfw3.h>

namespace Engine {

void EditorViewportSelectionController::update(
    const Input& input,
    const EditorViewport& viewport,
    const Camera2D& camera,
    Scene& scene,
    EditorSelectionState& selection)
{
    const bool leftMousePressed = input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
    const bool leftClick = leftMousePressed && !m_leftMouseWasPressed;
    m_leftMouseWasPressed = leftMousePressed;

    if (!leftMousePressed) {
        m_draggingSelection = false;
        m_draggedObjectId = InvalidSceneObjectId;
    }

    if ((!leftClick && !m_draggingSelection) || !viewport.hovered() || viewport.cameraMode() != EditorCameraMode::Orthographic2D) {
        return;
    }

    const std::optional<glm::vec2>& localMouse = viewport.localMousePosition();
    if (!localMouse) {
        return;
    }

    const glm::vec2 worldMouse = camera.screenToWorld(*localMouse);

    if (leftClick) {
        SceneObjectId targetObjectId = InvalidSceneObjectId;
        if (SceneObject* selected = scene.findObject(selection.selectedSceneObjectId())) {
            if (selected->sprite2D && containsPoint(centerHandleBounds(*selected, camera), worldMouse)) {
                targetObjectId = selected->id;
            }
        }

        if (targetObjectId == InvalidSceneObjectId) {
            targetObjectId = pickSpriteAt(scene, worldMouse);
        }

        selection.select(targetObjectId);
        if (SceneObject* target = scene.findObject(targetObjectId)) {
            m_draggingSelection = true;
            m_draggedObjectId = targetObjectId;
            m_dragOffset = glm::vec2{target->transform.position.x, target->transform.position.y} - worldMouse;
        }
        return;
    }

    if (m_draggingSelection) {
        SceneObject* dragged = scene.findObject(m_draggedObjectId);
        if (dragged == nullptr || !dragged->sprite2D) {
            m_draggingSelection = false;
            m_draggedObjectId = InvalidSceneObjectId;
            return;
        }

        const glm::vec2 targetPosition = worldMouse + m_dragOffset;
        dragged->transform.position.x = targetPosition.x;
        dragged->transform.position.y = targetPosition.y;
    }
}

void EditorViewportSelectionController::drawOverlay(
    Renderer2DWorld& renderer2DWorld,
    DebugRenderer& debugRenderer,
    const Scene& scene,
    const Camera2D& camera,
    const EditorSelectionState& selection) const
{
    const SceneObject* selected = scene.findObject(selection.selectedSceneObjectId());
    if (selected == nullptr || !selected->sprite2D) {
        return;
    }

    const SpriteBounds bounds = spriteBounds(*selected);
    (void)camera;

    renderer2DWorld.drawDebugFilledRect(bounds.minimum, bounds.size, {0.32f, 0.62f, 1.0f, 0.08f});
    debugRenderer.drawWireRect2D(bounds.minimum, bounds.size, {0.54f, 0.78f, 1.0f, 1.0f});
    debugRenderer.drawWireRect2D(
        bounds.minimum - glm::vec2{1.0f, 1.0f},
        bounds.size + glm::vec2{2.0f, 2.0f},
        {0.08f, 0.16f, 0.28f, 0.85f});
}

SceneObjectId EditorViewportSelectionController::pickSpriteAt(const Scene& scene, const glm::vec2& worldPosition) const
{
    const std::span<const SceneObject> objects = scene.objects();
    for (auto iterator = objects.rbegin(); iterator != objects.rend(); ++iterator) {
        const SceneObject& object = *iterator;
        if (!object.sprite2D) {
            continue;
        }

        if (containsPoint(spriteBounds(object), worldPosition)) {
            return object.id;
        }
    }

    return InvalidSceneObjectId;
}

EditorViewportSelectionController::SpriteBounds EditorViewportSelectionController::spriteBounds(const SceneObject& object) const
{
    if (!object.sprite2D) {
        return {};
    }

    const Sprite2DComponent& sprite = *object.sprite2D;
    const glm::vec2 size = sprite.size * glm::vec2{object.transform.scale.x, object.transform.scale.y};
    return {
        glm::vec2{object.transform.position.x, object.transform.position.y} - size * sprite.origin,
        size,
    };
}

EditorViewportSelectionController::SpriteBounds EditorViewportSelectionController::centerHandleBounds(
    const SceneObject& object,
    const Camera2D& camera) const
{
    constexpr float HandlePixels = 12.0f;
    const float safeZoom = std::max(camera.zoom, 0.001f);
    const float handleSize = HandlePixels / safeZoom;
    const glm::vec2 center{object.transform.position.x, object.transform.position.y};
    return {
        center - glm::vec2{handleSize * 0.5f, handleSize * 0.5f},
        {handleSize, handleSize},
    };
}

bool EditorViewportSelectionController::containsPoint(const SpriteBounds& bounds, const glm::vec2& worldPosition) const
{
    return worldPosition.x >= bounds.minimum.x &&
        worldPosition.y >= bounds.minimum.y &&
        worldPosition.x <= bounds.minimum.x + bounds.size.x &&
        worldPosition.y <= bounds.minimum.y + bounds.size.y;
}

} // namespace Engine
