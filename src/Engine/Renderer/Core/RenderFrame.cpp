#include "Engine/Renderer/Core/RenderFrame.hpp"

namespace Engine {

std::string_view renderStageName(const RenderStage stage)
{
    switch (stage) {
    case RenderStage::ShadowMaps: return "ShadowMaps";
    case RenderStage::Skybox: return "Skybox";
    case RenderStage::World3D: return "World3D";
    case RenderStage::Sprites: return "Sprites";
    case RenderStage::WorldSpaceUI: return "WorldSpaceUI";
    case RenderStage::GameHUD: return "GameHUD";
    case RenderStage::EditorOverlays: return "EditorOverlays";
    case RenderStage::Debug: return "Debug";
    case RenderStage::PostProcess: return "PostProcess";
    case RenderStage::EditorUI: return "EditorUI";
    }

    return "Unknown";
}

} // namespace Engine
