#pragma once

#include "Engine/Renderer/RenderFrame.hpp"

namespace Engine {

class PlaceholderRenderModule : public RenderModule {
public:
    void beginFrame(const RenderFrameContext& context) override;
    void resize(const glm::uvec2& swapchainSize) override;
    void recordCommands(RenderCommandRecorder& recorder, const RenderFrameContext& context) override;
    void endFrame() override;
    void releaseResources() override;

protected:
    [[nodiscard]] const RenderFrameContext& frameContext() const;
    [[nodiscard]] const glm::uvec2& lastResize() const;

private:
    RenderFrameContext m_frameContext{};
    glm::uvec2 m_lastResize{0U, 0U};
};

} // namespace Engine
