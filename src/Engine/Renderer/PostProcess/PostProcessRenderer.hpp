#pragma once

#include "Engine/Renderer/Core/RenderModuleBase.hpp"

namespace Engine {

class PostProcessRenderer final : public PlaceholderRenderModule {
public:
    [[nodiscard]] std::string_view name() const override;
    [[nodiscard]] RenderStage stage() const override;
};

} // namespace Engine
