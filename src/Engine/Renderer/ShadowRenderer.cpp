#include "Engine/Renderer/ShadowRenderer.hpp"

namespace Engine {

std::string_view ShadowRenderer::name() const
{
    return "ShadowRenderer";
}

RenderStage ShadowRenderer::stage() const
{
    return RenderStage::ShadowMaps;
}

} // namespace Engine
