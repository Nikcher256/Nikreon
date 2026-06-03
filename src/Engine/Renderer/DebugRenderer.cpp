#include "Engine/Renderer/DebugRenderer.hpp"

namespace Engine {

std::string_view DebugRenderer::name() const
{
    return "DebugRenderer";
}

RenderStage DebugRenderer::stage() const
{
    return RenderStage::Debug;
}

} // namespace Engine
