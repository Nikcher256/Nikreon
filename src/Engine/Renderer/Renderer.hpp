#pragma once

#include <memory>

#include <glm/vec2.hpp>

#include "Engine/Editor/EditorViewport.hpp"
#include "Engine/Renderer/RenderFrame.hpp"
#include "Engine/Renderer/RenderPipeline.hpp"
#include "Engine/Renderer/Vulkan/VulkanContext.hpp"

namespace Engine {

class Renderer2D;
class TextRenderer;
class VulkanRenderer2D;
class VulkanTextRenderer;
class Window;

class Renderer {
public:
    explicit Renderer(Window& window);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    [[nodiscard]] bool beginFrame();
    void endFrame();

    [[nodiscard]] Renderer2D& renderer2D();
    [[nodiscard]] Renderer2DWorld& renderer2DWorld();
    [[nodiscard]] TextRenderer& textRenderer();
    [[nodiscard]] glm::uvec2 viewportSize() const;
    void setEditorViewport(const EditorViewportPresentation& presentation, EditorViewportMode mode);

private:
    void createBackendRenderers();
    void recreateSwapchainResources();
    void prepareViewportWorldRenderer();
    [[nodiscard]] RenderFrameContext frameContext() const;

    VulkanContext m_context;
    RenderPipeline m_renderPipeline;
    std::unique_ptr<VulkanRenderer2D> m_renderer2D;
    std::unique_ptr<VulkanRenderer2D> m_viewportRenderer2D;
    std::unique_ptr<VulkanTextRenderer> m_textRenderer;
    EditorViewportPresentation m_editorViewportPresentation{};
    EditorViewportMode m_editorViewportMode{EditorViewportMode::Edit};
};

} // namespace Engine
