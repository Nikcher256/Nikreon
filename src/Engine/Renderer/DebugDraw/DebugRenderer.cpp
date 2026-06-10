#include "Engine/Renderer/DebugDraw/DebugRenderer.hpp"

#include <algorithm>
#include <cmath>

#include <glm/vec2.hpp>

namespace Engine {

std::string_view DebugRenderer::name() const
{
    return "DebugRenderer";
}

RenderStage DebugRenderer::stage() const
{
    return RenderStage::Debug;
}

void DebugRenderer::beginFrame(const RenderFrameContext& context)
{
    (void)context;
    m_gridRequest = {};
    m_lineVertices.clear();
}

void DebugRenderer::resize(const glm::uvec2& swapchainSize)
{
    (void)swapchainSize;
}

void DebugRenderer::recordCommands(RenderCommandRecorder& recorder, const RenderFrameContext& context)
{
    (void)context;
    recorder.beginStage(stage());
    recorder.endStage(stage());
}

void DebugRenderer::endFrame()
{
}

void DebugRenderer::releaseResources()
{
    m_lineVertices.clear();
}

void DebugRenderer::drawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color)
{
    m_lineVertices.push_back({start, color});
    m_lineVertices.push_back({end, color});
}

void DebugRenderer::drawGrid(const DebugGridRequest& request)
{
    m_gridRequest = request;
    m_gridRequest.enabled = true;
    m_gridRequest.step = std::max(m_gridRequest.step, 0.001f);
    m_gridRequest.majorEvery = std::max(m_gridRequest.majorEvery, 1.0f);
    m_gridRequest.fadeEnd = std::max(m_gridRequest.fadeEnd, m_gridRequest.fadeStart + 0.001f);
}

void DebugRenderer::drawWireRect2D(const glm::vec2& minimum, const glm::vec2& size, const glm::vec4& color)
{
    const glm::vec3 topLeft{minimum.x, minimum.y, 0.0f};
    const glm::vec3 topRight{minimum.x + size.x, minimum.y, 0.0f};
    const glm::vec3 bottomRight{minimum.x + size.x, minimum.y + size.y, 0.0f};
    const glm::vec3 bottomLeft{minimum.x, minimum.y + size.y, 0.0f};

    drawLine(topLeft, topRight, color);
    drawLine(topRight, bottomRight, color);
    drawLine(bottomRight, bottomLeft, color);
    drawLine(bottomLeft, topLeft, color);
}

void DebugRenderer::drawWireBox(const glm::vec3& minimum, const glm::vec3& maximum, const glm::vec4& color)
{
    const glm::vec3 c000{minimum.x, minimum.y, minimum.z};
    const glm::vec3 c100{maximum.x, minimum.y, minimum.z};
    const glm::vec3 c110{maximum.x, maximum.y, minimum.z};
    const glm::vec3 c010{minimum.x, maximum.y, minimum.z};

    const glm::vec3 c001{minimum.x, minimum.y, maximum.z};
    const glm::vec3 c101{maximum.x, minimum.y, maximum.z};
    const glm::vec3 c111{maximum.x, maximum.y, maximum.z};
    const glm::vec3 c011{minimum.x, maximum.y, maximum.z};

    drawLine(c000, c100, color);
    drawLine(c100, c110, color);
    drawLine(c110, c010, color);
    drawLine(c010, c000, color);

    drawLine(c001, c101, color);
    drawLine(c101, c111, color);
    drawLine(c111, c011, color);
    drawLine(c011, c001, color);

    drawLine(c000, c001, color);
    drawLine(c100, c101, color);
    drawLine(c110, c111, color);
    drawLine(c010, c011, color);
}

void DebugRenderer::drawCircleXY(const glm::vec3& center, const float radius, const glm::vec4& color, const std::uint32_t segments)
{
    const std::uint32_t safeSegments = std::max(segments, 3U);
    constexpr float Tau = 6.28318530718f;

    for (std::uint32_t index = 0; index < safeSegments; ++index) {
        const float a0 = Tau * static_cast<float>(index) / static_cast<float>(safeSegments);
        const float a1 = Tau * static_cast<float>(index + 1U) / static_cast<float>(safeSegments);

        drawLine(
            {center.x + std::cos(a0) * radius, center.y + std::sin(a0) * radius, center.z},
            {center.x + std::cos(a1) * radius, center.y + std::sin(a1) * radius, center.z},
            color);
    }
}

std::span<const DebugLineVertex> DebugRenderer::lineVertices() const
{
    return m_lineVertices;
}

const DebugGridRequest& DebugRenderer::gridRequest() const
{
    return m_gridRequest;
}

} // namespace Engine
