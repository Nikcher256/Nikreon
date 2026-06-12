#include "Engine/Editor/EditorViewportOverlayController.hpp"

#include "Engine/Renderer/DebugDraw/DebugRenderer.hpp"

namespace Engine {

void EditorViewportOverlayController::draw(
    DebugRenderer& debugRenderer,
    const EditorViewport& viewport,
    const Renderer2DWorldCamera& camera) const
{
    (void)camera;

    if (!viewport.gridVisible() || viewport.mode() == EditorViewportMode::Play) {
        return;
    }

    debugRenderer.drawGrid({
        .enabled = true,
        .step = 10.0f,
        .majorEvery = 10.0f,
        .fadeStart = 0.0f,
        .fadeEnd = 3100.0f,
        .fadeMinimumAlpha = 0.005f,
    });
}

} // namespace Engine
