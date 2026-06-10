#pragma once

#include "Engine/Editor/EditorViewport.hpp"
#include "Engine/Renderer/World2D/Renderer2DWorld.hpp"

namespace Engine {

class DebugRenderer;

class EditorViewportOverlayController {
public:
    void draw(DebugRenderer& debugRenderer, const EditorViewport& viewport, const Renderer2DWorldCamera& camera) const;

private:
};

} // namespace Engine
