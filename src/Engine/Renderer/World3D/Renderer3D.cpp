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

} // namespace Engine
