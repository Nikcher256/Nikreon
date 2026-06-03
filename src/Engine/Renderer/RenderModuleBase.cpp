#include "Engine/Renderer/RenderModuleBase.hpp"

namespace Engine {

void PlaceholderRenderModule::beginFrame(const RenderFrameContext& context)
{
    m_frameContext = context;
}

void PlaceholderRenderModule::resize(const glm::uvec2& swapchainSize)
{
    m_lastResize = swapchainSize;
}

void PlaceholderRenderModule::recordCommands(RenderCommandRecorder& recorder, const RenderFrameContext& context)
{
    (void)context;
    recorder.beginStage(stage());
    recorder.endStage(stage());
}

void PlaceholderRenderModule::endFrame()
{
}

void PlaceholderRenderModule::releaseResources()
{
}

const RenderFrameContext& PlaceholderRenderModule::frameContext() const
{
    return m_frameContext;
}

const glm::uvec2& PlaceholderRenderModule::lastResize() const
{
    return m_lastResize;
}

} // namespace Engine
