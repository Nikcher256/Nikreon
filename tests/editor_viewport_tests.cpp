#include "Engine/Editor/EditorViewport.hpp"

#include <cassert>

using namespace Engine;

int main()
{
    EditorViewport viewport;
    const EditorViewportPresentation panel{{100.0f, 50.0f}, {320.0f, 180.0f}, {0.2f, 0.4f, 0.6f, 1.0f}};

    viewport.updateInteraction(panel, {140.0f, 90.0f}, true);
    assert(viewport.presentation().clearColor == glm::vec4(0.2f, 0.4f, 0.6f, 1.0f));
    assert(viewport.hovered());
    assert(viewport.focused());
    assert(viewport.localMousePosition() == glm::vec2(40.0f, 40.0f));
    assert(viewport.pickingCoordinates() == glm::uvec2(40U, 40U));

    viewport.updateEditorCamera(1.0f, {.moveForward = 1.0f});
    assert(viewport.editorCameraPosition() == glm::vec3(0.0f, 0.0f, 0.0f));

    viewport.setMode(EditorViewportMode::Play);
    viewport.updateEditorCamera(1.0f, {.moveRight = 1.0f});
    assert(viewport.editorCameraPosition() == glm::vec3(0.0f, 0.0f, 0.0f));

    viewport.updateInteraction(panel, {10.0f, 10.0f}, false);
    viewport.updateInteraction(panel, {10.0f, 10.0f}, true);
    assert(!viewport.hovered());
    assert(!viewport.focused());
    assert(!viewport.localMousePosition().has_value());
    assert(!viewport.pickingCoordinates().has_value());

    viewport.updateInteraction({{20.0f, 10.0f}, {640.0f, 360.0f}}, {659.0f, 369.0f}, false);
    assert(viewport.hovered());
    assert(viewport.localMousePosition() == glm::vec2(639.0f, 359.0f));
    assert(viewport.pickingCoordinates() == glm::uvec2(639U, 359U));

    assert(editorViewportModeName(EditorViewportMode::HudEdit) == "HudEdit");
    return 0;
}
