#pragma once

#include "Engine/Editor/EditorSelectionState.hpp"
#include "Engine/Editor/EditorViewport.hpp"
#include "Engine/Editor/PickingSystem.hpp"
#include "Engine/Renderer/Camera/Camera2D.hpp"
#include "Engine/Scene/Scene.hpp"

#include <glm/vec2.hpp>

namespace Engine {

class DebugRenderer;
class Input;
class SpriteRenderer;

class EditorViewportSelectionController {
public:
    void clearDrag();
    void update(
        const Input& input,
        const EditorViewport& viewport,
        const Camera2D& camera,
        Scene& scene,
        EditorSelectionState& selection);
    void drawOverlay(
        SpriteRenderer& spriteRenderer,
        DebugRenderer& debugRenderer,
        const Scene& scene,
        const Camera2D& camera,
        const EditorSelectionState& selection) const;

private:
    struct SpriteBounds {
        glm::vec2 minimum{0.0f, 0.0f};
        glm::vec2 size{0.0f, 0.0f};
    };

    [[nodiscard]] SpriteBounds centerHandleBounds(const SceneObject& object, const Camera2D& camera) const;
    [[nodiscard]] bool containsPoint(const SpriteBounds& bounds, const glm::vec2& worldPosition) const;

    bool m_leftMouseWasPressed{false};
    bool m_draggingSelection{false};
    SceneObjectId m_draggedObjectId{InvalidSceneObjectId};
    glm::vec2 m_dragOffset{0.0f, 0.0f};
};

} // namespace Engine
