#include "Engine/Renderer/Renderer.hpp"

#include "Engine/Renderer/Renderer2D.hpp"
#include "Engine/Renderer/Renderer2DWorld.hpp"
#include "Engine/Renderer/TextRenderer.hpp"
#include "Engine/Renderer/Vulkan/VulkanRenderer2D.hpp"
#include "Engine/Renderer/Vulkan/VulkanTextRenderer.hpp"

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
    m_viewportRenderer2D.reset();
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
    prepareViewportWorldRenderer();
    const RenderFrameContext context = frameContext();
    const VulkanContext::FrameResult frameResult = m_context.drawFrame(*m_renderer2D, *m_viewportRenderer2D, *m_textRenderer, m_renderPipeline, context);
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

void Renderer::createBackendRenderers()
{
    m_renderer2D = std::make_unique<VulkanRenderer2D>(
        m_context.device(),
        m_context.physicalDevice(),
        m_context.renderPass());
    m_viewportRenderer2D = std::make_unique<VulkanRenderer2D>(
        m_context.device(),
        m_context.physicalDevice(),
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
    m_context.waitIdle();
    m_textRenderer.reset();
    m_viewportRenderer2D.reset();
    m_renderer2D.reset();
    m_context.recreateSwapchain();
    createBackendRenderers();
    m_renderPipeline.resize(m_context.swapchainSize());
}

void Renderer::prepareViewportWorldRenderer()
{
    const glm::uvec2 targetSize = m_context.viewportRenderTargetSize();
    m_viewportRenderer2D->begin(targetSize);

    const Renderer2DWorld& worldRenderer = m_renderPipeline.renderer2DWorld();
    const Renderer2DWorldCamera& camera = worldRenderer.camera();
    const glm::vec2 targetCenter{
        static_cast<float>(targetSize.x) * 0.5f,
        static_cast<float>(targetSize.y) * 0.5f,
    };

    const auto toTarget2D = [&camera, targetCenter](const glm::vec3& worldPosition) {
        return glm::vec2{
            targetCenter.x + (worldPosition.x - camera.camera2D.position.x) * camera.camera2D.zoom,
            targetCenter.y - (worldPosition.y - camera.camera2D.position.y) * camera.camera2D.zoom,
        };
    };

    const glm::mat4 worldToClip = camera.camera3D.viewProjection();
    const auto toTarget3D = [&worldToClip, targetSize](const glm::vec3& worldPosition) -> std::optional<glm::vec2> {
        const glm::vec4 clip = worldToClip * glm::vec4{worldPosition, 1.0f};
        if (clip.w <= 0.0001f) {
            return std::nullopt;
        }

        const glm::vec3 ndc = glm::vec3{clip} / clip.w;
        return glm::vec2{
            (ndc.x * 0.5f + 0.5f) * static_cast<float>(targetSize.x),
            (0.5f - ndc.y * 0.5f) * static_cast<float>(targetSize.y),
        };
    };

    const auto toTarget = [&camera, &toTarget2D, &toTarget3D](const glm::vec3& worldPosition) -> std::optional<glm::vec2> {
        if (camera.mode == Renderer2DWorldCameraMode::Perspective3D) {
            return toTarget3D(worldPosition);
        }

        return toTarget2D(worldPosition);
    };

    const std::span<const WorldQuadVertex> vertices = worldRenderer.vertices();
    for (std::size_t index = 0; index + 3 < vertices.size(); index += 4) {
        glm::vec2 minimum{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
        glm::vec2 maximum{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
        for (std::size_t corner = 0; corner < 4; ++corner) {
            const auto targetPosition = toTarget(vertices[index + corner].position);
            if (!targetPosition) {
                minimum = {};
                maximum = {};
                break;
            }

            minimum = glm::min(minimum, *targetPosition);
            maximum = glm::max(maximum, *targetPosition);
        }

        if (minimum == glm::vec2{} && maximum == glm::vec2{}) {
            continue;
        }

        const glm::vec2 size = glm::max(maximum - minimum, glm::vec2{1.0f, 1.0f});
        glm::vec4 color = vertices[index].color;
        color.a = std::clamp(color.a, 0.1f, 1.0f);
        m_viewportRenderer2D->drawSdfRect(minimum, size, 2.0f, color, {1.0f, 1.0f, 1.0f, 0.18f}, 1.0f);
    }

    for (const WorldDebugLine& line : worldRenderer.debugLines()) {
        const auto start = toTarget(line.start);
        const auto end = toTarget(line.end);
        if (!start || !end) {
            continue;
        }
        const glm::vec2 minimum = glm::min(*start, *end);
        const glm::vec2 maximum = glm::max(*start, *end);
        const glm::vec2 size = glm::max(maximum - minimum, glm::vec2{line.thickness, line.thickness});
        m_viewportRenderer2D->drawQuad(minimum, size, line.color);
    }

    m_viewportRenderer2D->end();
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
