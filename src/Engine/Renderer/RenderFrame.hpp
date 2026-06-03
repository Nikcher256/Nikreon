#pragma once

#include <string_view>

#include <glm/vec2.hpp>

#include "Engine/Editor/EditorViewport.hpp"

namespace Engine {

enum class RenderStage {
    ShadowMaps,
    Skybox,
    World3D,
    World2D,
    WorldSpaceUI,
    GameHUD,
    EditorOverlays,
    Debug,
    PostProcess,
    EditorUI,
};

[[nodiscard]] std::string_view renderStageName(RenderStage stage);

struct RenderFrameContext {
    glm::uvec2 swapchainSize{0U, 0U};
    EditorViewportPresentation editorViewport{};
    EditorViewportMode viewportMode{EditorViewportMode::Edit};
};

class RenderCommandRecorder {
public:
    virtual ~RenderCommandRecorder() = default;

    virtual void beginStage(RenderStage stage) = 0;
    virtual void endStage(RenderStage stage) = 0;
};

class RenderModule {
public:
    virtual ~RenderModule() = default;

    [[nodiscard]] virtual std::string_view name() const = 0;
    [[nodiscard]] virtual RenderStage stage() const = 0;
    virtual void beginFrame(const RenderFrameContext& context) = 0;
    virtual void resize(const glm::uvec2& swapchainSize) = 0;
    virtual void recordCommands(RenderCommandRecorder& recorder, const RenderFrameContext& context) = 0;
    virtual void endFrame() = 0;
    virtual void releaseResources() = 0;
};

} // namespace Engine
