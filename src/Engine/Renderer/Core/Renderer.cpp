#include "Engine/Renderer/Core/Renderer.hpp"

#include "Engine/Renderer/Renderer2D.hpp"
#include "Engine/Renderer/World2D/Renderer2DWorld.hpp"
#include "Engine/Renderer/TextRenderer.hpp"
#include "Engine/Renderer/Vulkan/VulkanRenderer2D.hpp"
#include "Engine/Renderer/Vulkan/VulkanTextRenderer.hpp"
#include "Engine/Renderer/Vulkan/World2D/VulkanRenderer2DWorld.hpp"

#include <array>
#include <algorithm>
#include <filesystem>
#include <limits>
#include <span>
#include <optional>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/common.hpp>
#include <spdlog/spdlog.h>

namespace Engine {

#ifndef NIKREON_ASSET_DIR
#define NIKREON_ASSET_DIR "assets"
#endif

Renderer::Renderer(Window& window)
    : m_context(window)
{
    createBackendRenderers();
    m_renderPipeline.resize(m_context.swapchainSize());
}

Renderer::~Renderer()
{
    m_context.waitIdle();
    m_textRenderer.reset();
    m_viewportRenderer2DWorld.reset();
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

    const RenderFrameContext context = frameContext();
    m_renderPipeline.beginFrame(context);
    renderer2D().begin(m_context.swapchainSize());
    textRenderer().begin(m_context.swapchainSize());
    return true;
}

void Renderer::endFrame()
{
    renderer2D().end();
    textRenderer().end();
    prepareViewportWorld2DRenderer();
    const RenderFrameContext context = frameContext();
    const VulkanContext::FrameResult frameResult = m_context.drawFrame(*m_renderer2D, *m_viewportRenderer2DWorld, *m_textRenderer, m_renderPipeline, context);
    m_renderPipeline.endFrame();
    if (frameResult == VulkanContext::FrameResult::RecreateSwapchain) {
        recreateSwapchainResources();
    }
}

Renderer2D& Renderer::renderer2D()
{
    return *m_renderer2D;
}

Renderer2DWorld& Renderer::renderer2DWorld()
{
    return m_renderPipeline.renderer2DWorld();
}

TextRenderer& Renderer::textRenderer()
{
    return *m_textRenderer;
}

ResourceManager& Renderer::resources()
{
    return m_resources;
}

const ResourceManager& Renderer::resources() const
{
    return m_resources;
}

glm::uvec2 Renderer::viewportSize() const
{
    return m_context.swapchainSize();
}

void Renderer::setEditorViewport(const EditorViewportPresentation& presentation, const EditorViewportMode mode)
{
    m_editorViewportPresentation = presentation;
    m_editorViewportMode = mode;
    m_context.setEditorViewport(presentation, mode);
}

void Renderer::setPresentMode(const VulkanContext::PresentMode presentMode)
{
    if (m_context.setPresentMode(presentMode)) {
        recreateSwapchainResources();
    }
}

VulkanContext::PresentMode Renderer::presentMode() const
{
    return m_context.presentMode();
}

void Renderer::createBackendRenderers()
{
    m_renderer2D = std::make_unique<VulkanRenderer2D>(
        m_context.device(),
        m_context.physicalDevice(),
        m_context.graphicsQueue(),
        m_context.commandPool(),
        m_context.renderPass());
    m_viewportRenderer2DWorld = std::make_unique<VulkanRenderer2DWorld>(
        m_context.device(),
        m_context.physicalDevice(),
        m_context.graphicsQueue(),
        m_context.commandPool(),
        m_context.viewportRenderPass());
    m_textRenderer = std::make_unique<VulkanTextRenderer>(
        m_context.device(),
        m_context.physicalDevice(),
        m_context.graphicsQueue(),
        m_context.commandPool(),
        m_context.renderPass());

    const std::array<std::filesystem::path, 5> fontCandidates = {
        NIKREON_ASSET_DIR "/fonts/NotoSans-Regular.ttf",
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
    const bool renderPassChanged = m_context.recreateSwapchain();
    if (renderPassChanged) {
        m_context.waitIdle();
        m_textRenderer.reset();
        m_viewportRenderer2DWorld.reset();
        m_renderer2D.reset();
        createBackendRenderers();
    }
    m_renderPipeline.resize(m_context.swapchainSize());
}

void Renderer::prepareViewportWorld2DRenderer()
{
    const glm::uvec2 targetSize = m_context.viewportRenderTargetSize();
    m_viewportRenderer2DWorld->begin(targetSize);
    m_viewportRenderer2DWorld->submit(m_renderPipeline.renderer2DWorld(), m_resources);
    m_viewportRenderer2DWorld->end();
}

RenderFrameContext Renderer::frameContext() const
{
    return {
        m_context.swapchainSize(),
        m_editorViewportPresentation,
        m_editorViewportMode,
    };
}

} // namespace Engine
