#pragma once

#include <array>
#include <span>

#include "Engine/Renderer/DebugRenderer.hpp"
#include "Engine/Renderer/NikreonUIRenderAdapter.hpp"
#include "Engine/Renderer/PostProcessRenderer.hpp"
#include "Engine/Renderer/Renderer2DWorld.hpp"
#include "Engine/Renderer/Renderer3D.hpp"
#include "Engine/Renderer/ShadowRenderer.hpp"
#include "Engine/Renderer/SkyboxRenderer.hpp"

namespace Engine {

class RenderPipeline {
public:
    RenderPipeline();
    ~RenderPipeline();

    RenderPipeline(const RenderPipeline&) = delete;
    RenderPipeline& operator=(const RenderPipeline&) = delete;
    RenderPipeline(RenderPipeline&&) = delete;
    RenderPipeline& operator=(RenderPipeline&&) = delete;

    void beginFrame(const RenderFrameContext& context);
    void resize(const glm::uvec2& swapchainSize);
    void recordCommands(RenderCommandRecorder& recorder, const RenderFrameContext& context);
    void endFrame();
    void releaseResources();

    [[nodiscard]] Renderer2DWorld& renderer2DWorld();
    [[nodiscard]] const Renderer2DWorld& renderer2DWorld() const;
    [[nodiscard]] std::span<const RenderStage> stageSequence() const;

private:
    [[nodiscard]] RenderModule* moduleForStage(RenderStage stage);

    ShadowRenderer m_shadowRenderer;
    SkyboxRenderer m_skyboxRenderer;
    Renderer3D m_renderer3D;
    Renderer2DWorld m_renderer2DWorld;
    DebugRenderer m_debugRenderer;
    PostProcessRenderer m_postProcessRenderer;
    NikreonUIRenderAdapter m_uiRenderAdapter;
    std::array<RenderStage, 10> m_stageSequence;
};

} // namespace Engine
