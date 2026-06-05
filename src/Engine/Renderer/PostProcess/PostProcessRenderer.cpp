#include "Engine/Renderer/PostProcess/PostProcessRenderer.hpp"

namespace Engine {

std::string_view PostProcessRenderer::name() const
{
    return "PostProcessRenderer";
}

RenderStage PostProcessRenderer::stage() const
{
    return RenderStage::PostProcess;
}

} // namespace Engine
