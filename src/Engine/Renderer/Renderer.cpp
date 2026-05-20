#include "Engine/Renderer/Renderer.hpp"

#include "Engine/Renderer/Renderer2D.hpp"
#include "Engine/Renderer/Vulkan/VulkanRenderer2D.hpp"

namespace Engine {

Renderer::Renderer(Window& window)
    : m_context(window)
{
    createBackendRenderers();
}

Renderer::~Renderer()
{
    m_context.waitIdle();
    m_renderer2D.reset();
}

bool Renderer::beginFrame()
{
    if (!m_context.isDrawable()) {
        return false;
    }

    if (m_context.shouldRecreateSwapchain()) {
        recreateSwapchainResources();
    }

    renderer2D().begin(m_context.swapchainSize());
    return true;
}

void Renderer::endFrame()
{
    renderer2D().end();
    const VulkanContext::FrameResult frameResult = m_context.drawFrame(*m_renderer2D);
    if (frameResult == VulkanContext::FrameResult::RecreateSwapchain) {
        recreateSwapchainResources();
    }
}

Renderer2D& Renderer::renderer2D()
{
    return *m_renderer2D;
}

glm::uvec2 Renderer::viewportSize() const
{
    return m_context.swapchainSize();
}

void Renderer::createBackendRenderers()
{
    m_renderer2D = std::make_unique<VulkanRenderer2D>(
        m_context.device(),
        m_context.physicalDevice(),
        m_context.renderPass());
}

void Renderer::recreateSwapchainResources()
{
    m_context.waitIdle();
    m_renderer2D.reset();
    m_context.recreateSwapchain();
    createBackendRenderers();
}

} // namespace Engine
