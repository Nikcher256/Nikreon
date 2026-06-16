#pragma once

#include "Engine/Renderer/Core/RenderModuleBase.hpp"
#include "Engine/Renderer/Core/WorldRenderView.hpp"

namespace Engine {

class Renderer3D final : public PlaceholderRenderModule {
public:
    [[nodiscard]] std::string_view name() const override;
    [[nodiscard]] RenderStage stage() const override;

    void setView(const WorldRenderView& view);
    [[nodiscard]] const WorldRenderView& view() const;

private:
    WorldRenderView m_view{};
};

} // namespace Engine
