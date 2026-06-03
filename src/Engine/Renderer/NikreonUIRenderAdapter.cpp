#include "Engine/Renderer/NikreonUIRenderAdapter.hpp"

namespace Engine {

std::string_view NikreonUIRenderAdapter::name() const
{
    return "NikreonUIRenderAdapter";
}

RenderStage NikreonUIRenderAdapter::stage() const
{
    return RenderStage::EditorUI;
}

} // namespace Engine
