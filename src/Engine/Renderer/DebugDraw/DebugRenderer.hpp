#pragma once

#include "Engine/Renderer/Core/RenderModuleBase.hpp"

#include <span>
#include <vector>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Engine {

struct DebugLineVertex {
    glm::vec3 position{0.0f};
    glm::vec4 color{1.0f};
};

struct DebugGridRequest {
    bool enabled{false};
    float step{10.0f};
    float majorEvery{10.0f};
    float fadeStart{350.0f};
    float fadeEnd{850.0f};
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
