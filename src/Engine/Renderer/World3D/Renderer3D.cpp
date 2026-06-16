#include "Engine/Renderer/World3D/Renderer3D.hpp"

namespace Engine {

std::string_view Renderer3D::name() const
{
    return "Renderer3D";
}

RenderStage Renderer3D::stage() const
{
    return RenderStage::World3D;
}

void Renderer3D::setView(const WorldRenderView& view)
{
    m_view = view;
}

const WorldRenderView& Renderer3D::view() const
{
    return m_view;
}

} // namespace Engine
