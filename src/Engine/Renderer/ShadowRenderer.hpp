#pragma once

#include "Engine/Renderer/RenderModuleBase.hpp"

namespace Engine {

class ShadowRenderer final : public PlaceholderRenderModule {
public:
    [[nodiscard]] std::string_view name() const override;
    [[nodiscard]] RenderStage stage() const override;
};

} // namespace Engine
