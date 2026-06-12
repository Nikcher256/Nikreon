#pragma once

#include "Engine/Renderer/Core/RenderModuleBase.hpp"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Engine {

struct DebugLineVertex {
    glm::vec3 position{0.0f};
    glm::vec4 color{1.0f};
};

struct DebugGridRequest {
    bool enabled{false};

    // Distance between small grid lines.
    float step{10.0f};

    // Every Nth line is treated as major.
    float majorEvery{10.0f};

    // Grid is fully visible until this distance.
    float fadeStart{1000.0f};

    // Grid smoothly fades until this distance.
    // 2100 - 100 = 2000 units of fade range.
    float fadeEnd{3100.0f};

    // Minimum alpha after fadeEnd.
    // 0.08 means the grid never becomes fully invisible.
    float fadeMinimumAlpha{0.0f};
};

class DebugRenderer final : public RenderModule {
public:
    [[nodiscard]] std::string_view name() const override;
    [[nodiscard]] RenderStage stage() const override;

    void beginFrame(const RenderFrameContext& context) override;
    void resize(const glm::uvec2& swapchainSize) override;
    void recordCommands(RenderCommandRecorder& recorder, const RenderFrameContext& context) override;
    void endFrame() override;
    void releaseResources() override;

    void drawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color);
    void drawGrid(const DebugGridRequest& request = {});
    void drawWireRect2D(const glm::vec2& minimum, const glm::vec2& size, const glm::vec4& color);
    void drawWireBox(const glm::vec3& minimum, const glm::vec3& maximum, const glm::vec4& color);
    void drawCircleXY(const glm::vec3& center, float radius, const glm::vec4& color, std::uint32_t segments = 48);

    [[nodiscard]] std::span<const DebugLineVertex> lineVertices() const;
    [[nodiscard]] const DebugGridRequest& gridRequest() const;

private:
    DebugGridRequest m_gridRequest;
    std::vector<DebugLineVertex> m_lineVertices;
};

} // namespace Engine
