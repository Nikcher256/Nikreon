#pragma once

#include "Engine/Editor/EditorViewport.hpp"
#include "Engine/Renderer/Sprite/SpriteRenderer.hpp"

namespace Engine {

class DebugRenderer;

class EditorViewportOverlayController {
public:
    void draw(DebugRenderer& debugRenderer, const EditorViewport& viewport, const SpriteRendererCamera& camera) const;

private:
};

} // namespace Engine
