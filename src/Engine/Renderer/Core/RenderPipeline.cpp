#include "Engine/Renderer/Core/RenderPipeline.hpp"

namespace Engine {

RenderPipeline::RenderPipeline()
    : m_stageSequence{
        RenderStage::ShadowMaps,
        RenderStage::Skybox,
        RenderStage::World3D,
        RenderStage::World2D,
        RenderStage::WorldSpaceUI,
        RenderStage::GameHUD,
        RenderStage::EditorOverlays,
        RenderStage::Debug,
        RenderStage::PostProcess,
        RenderStage::EditorUI,
    }
{
}

RenderPipeline::~RenderPipeline()
{
    releaseResources();
}

void RenderPipeline::beginFrame(const RenderFrameContext& context)
{
    for (const RenderStage stage : m_stageSequence) {
        if (RenderModule* module = moduleForStage(stage)) {
            module->beginFrame(context);
        }
    }
}

void RenderPipeline::resize(const glm::uvec2& swapchainSize)
{
    for (const RenderStage stage : m_stageSequence) {
        if (RenderModule* module = moduleForStage(stage)) {
            module->resize(swapchainSize);
        }
    }
}

void RenderPipeline::recordCommands(RenderCommandRecorder& recorder, const RenderFrameContext& context)
{
    for (const RenderStage stage : m_stageSequence) {
        if (RenderModule* module = moduleForStage(stage)) {
            module->recordCommands(recorder, context);
        } else {
            recorder.beginStage(stage);
            recorder.endStage(stage);
        }
    }
}

void RenderPipeline::endFrame()
{
    for (const RenderStage stage : m_stageSequence) {
        if (RenderModule* module = moduleForStage(stage)) {
            module->endFrame();
        }
    }
}

void RenderPipeline::releaseResources()
{
    for (const RenderStage stage : m_stageSequence) {
        if (RenderModule* module = moduleForStage(stage)) {
            module->releaseResources();
        }
    }
}

Renderer2DWorld& RenderPipeline::renderer2DWorld()
{
    return m_renderer2DWorld;
}

const Renderer2DWorld& RenderPipeline::renderer2DWorld() const
{
    return m_renderer2DWorld;
}

std::span<const RenderStage> RenderPipeline::stageSequence() const
{
    return m_stageSequence;
}

RenderModule* RenderPipeline::moduleForStage(const RenderStage stage)
{
    switch (stage) {
    case RenderStage::ShadowMaps: return &m_shadowRenderer;
    case RenderStage::Skybox: return &m_skyboxRenderer;
    case RenderStage::World3D: return &m_renderer3D;
    case RenderStage::World2D: return &m_renderer2DWorld;
    case RenderStage::Debug: return &m_debugRenderer;
    case RenderStage::PostProcess: return &m_postProcessRenderer;
    case RenderStage::EditorUI: return &m_uiRenderAdapter;
    case RenderStage::WorldSpaceUI:
    case RenderStage::GameHUD:
    case RenderStage::EditorOverlays:
        return nullptr;
    }

    return nullptr;
}

} // namespace Engine
