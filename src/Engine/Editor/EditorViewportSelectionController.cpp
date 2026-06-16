#include "Engine/Editor/EditorViewportSelectionController.hpp"

#include "Engine/Core/Input.hpp"
#include "Engine/Renderer/Core/WorldPicking.hpp"
#include "Engine/Renderer/DebugDraw/DebugRenderer.hpp"
#include "Engine/Renderer/Sprite/SpriteRenderer.hpp"

#include <algorithm>
#include <span>

#include <GLFW/glfw3.h>

namespace Engine {

void EditorViewportSelectionController::clearDrag()
{
    m_draggingSelection = false;
    m_draggedObjectId = InvalidSceneObjectId;
    m_dragOffset = {0.0f, 0.0f};
}

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
        clearDrag();
    }

    if ((!leftClick && !m_draggingSelection) || !viewport.hovered()) {
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
            if (selected->spriteRenderer && containsPoint(centerHandleBounds(*selected, camera), worldMouse)) {
                targetObjectId = selected->id;
            }
        }

        if (targetObjectId == InvalidSceneObjectId) {
            const WorldRenderView activeView = viewport.worldRenderView(camera.viewportSize);
            const Ray3D ray = worldRayFromScreenPoint(activeView, *localMouse);
            if (const std::optional<PickHit> hit = PickingSystem::pickScene(scene, ray)) {
                targetObjectId = hit->objectId;
            }
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
        if (dragged == nullptr || !dragged->spriteRenderer) {
            clearDrag();
            return;
        }

        const glm::vec2 targetPosition = worldMouse + m_dragOffset;
        dragged->transform.position.x = targetPosition.x;
        dragged->transform.position.y = targetPosition.y;
    }
}

void EditorViewportSelectionController::drawOverlay(
    SpriteRenderer& spriteRenderer,
    DebugRenderer& debugRenderer,
    const Scene& scene,
    const Camera2D& camera,
    const EditorSelectionState& selection) const
{
    const SceneObject* selected = scene.findObject(selection.selectedSceneObjectId());
    if (selected == nullptr || !selected->spriteRenderer) {
        return;
    }

    const PickingSystem::SpriteQuadCorners corners = PickingSystem::spriteQuadCorners(*selected);
    (void)spriteRenderer;
    (void)camera;

    debugRenderer.drawLine(corners[0], corners[1], {0.54f, 0.78f, 1.0f, 1.0f});
    debugRenderer.drawLine(corners[1], corners[2], {0.54f, 0.78f, 1.0f, 1.0f});
    debugRenderer.drawLine(corners[2], corners[3], {0.54f, 0.78f, 1.0f, 1.0f});
    debugRenderer.drawLine(corners[3], corners[0], {0.54f, 0.78f, 1.0f, 1.0f});
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
