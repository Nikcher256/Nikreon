#include "Engine/Renderer/Renderer.hpp"

#include "Engine/Renderer/Renderer2D.hpp"
#include "Engine/Renderer/TextRenderer.hpp"
#include "Engine/Renderer/Vulkan/VulkanRenderer2D.hpp"
#include "Engine/Renderer/Vulkan/VulkanTextRenderer.hpp"

#include <array>
#include <filesystem>

#include <spdlog/spdlog.h>

namespace Engine {

Renderer::Renderer(Window& window)
    : m_context(window)
{
    createBackendRenderers();
}

Renderer::~Renderer()
{
    m_context.waitIdle();
    m_textRenderer.reset();
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
    textRenderer().begin(m_context.swapchainSize());
    return true;
}

void Renderer::endFrame()
{
    renderer2D().end();
    textRenderer().end();
    const VulkanContext::FrameResult frameResult = m_context.drawFrame(*m_renderer2D, *m_textRenderer);
    if (frameResult == VulkanContext::FrameResult::RecreateSwapchain) {
        recreateSwapchainResources();
    }
}

Renderer2D& Renderer::renderer2D()
{
    return *m_renderer2D;
}

TextRenderer& Renderer::textRenderer()
{
    return *m_textRenderer;
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
    m_textRenderer = std::make_unique<VulkanTextRenderer>(
        m_context.device(),
        m_context.physicalDevice(),
        m_context.graphicsQueue(),
        m_context.commandPool(),
        m_context.renderPass());

    const std::array<std::filesystem::path, 4> fontCandidates = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
    };

    for (const auto& path : fontCandidates) {
        if (std::filesystem::exists(path) && m_textRenderer->loadFont("default", path, 16.0f)) {
            return;
        }
    }

    spdlog::warn("TextRenderer could not find a default system font. Editor labels will be hidden.");
}

void Renderer::recreateSwapchainResources()
{
    m_context.waitIdle();
    m_textRenderer.reset();
    m_renderer2D.reset();
    m_context.recreateSwapchain();
    createBackendRenderers();
}

} // namespace Engine
