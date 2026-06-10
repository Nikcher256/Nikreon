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
        .fadeStart = 1400.0f,
        .fadeEnd = 2400.0f,
    });
}

} // namespace Engine
