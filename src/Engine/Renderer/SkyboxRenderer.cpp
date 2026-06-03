#include "Engine/Renderer/SkyboxRenderer.hpp"

namespace Engine {

std::string_view SkyboxRenderer::name() const
{
    return "SkyboxRenderer";
}

RenderStage SkyboxRenderer::stage() const
{
    return RenderStage::Skybox;
}

} // namespace Engine
