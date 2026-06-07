#include "Engine/Core/Application.hpp"

#include "Engine/Editor/EditorLayer.hpp"

#include <memory>

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

namespace Engine {

Application::Application()
    : m_window(WindowProps{
          .title = "Nikreon Engine",
          .width = 1280,
          .height = 720,
      })
    , m_input(m_window)
    , m_renderer(m_window)
{
    m_time.reset();
    auto editorLayer = std::make_unique<EditorLayer>();
    m_editorLayer = editorLayer.get();
    m_layers.pushLayer(std::move(editorLayer));

    m_window.setRefreshCallback([this]() {
        render();
    });

    spdlog::info("Application created.");
}

Application::~Application()
{
    spdlog::info("Application destroyed.");
}

// Runs the main engine loop until the window closes or a frame cap is reached.
void Application::run(const RunOptions& options)
{
    spdlog::info("Application loop started.");

    while (!m_window.shouldClose()) {
        m_time.tick();
        m_window.pollEvents();

        update(m_time.deltaSeconds());
        render();
        m_window.clearTransientInput();

        ++m_frameCount;
        if (options.maxFrames > 0 && m_frameCount >= options.maxFrames) {
            m_window.requestClose();
        }
    }

    spdlog::info("Application loop stopped after {:.2f}s.", m_time.elapsedSeconds());
}

// Updates application-level input shortcuts and future game/editor systems.
void Application::update(const float deltaTime)
{
    (void)deltaTime;

    const bool altPressed =
        m_input.isKeyPressed(GLFW_KEY_LEFT_ALT) ||
        m_input.isKeyPressed(GLFW_KEY_RIGHT_ALT);
    const bool fullscreenTogglePressed =
        m_input.isKeyPressed(GLFW_KEY_F11) ||
        (altPressed && m_input.isKeyPressed(GLFW_KEY_ENTER));

    if (fullscreenTogglePressed && !m_fullscreenToggleWasPressed) {
        m_window.toggleFullscreen();
    }

    m_fullscreenToggleWasPressed = fullscreenTogglePressed;
    m_layers.update(deltaTime, m_input);
}

// Records and presents one editor frame if the swapchain is drawable.
void Application::render()
{
    if (m_renderingFrame) {
        return;
    }

    m_renderingFrame = true;

    if (m_renderer.beginFrame()) {
        m_layers.render(
            m_renderer.renderer2D(),
            m_renderer.renderer2DWorld(),
            m_renderer.textRenderer(),
            m_renderer.resources(),
            m_renderer.viewportSize(),
            m_input);
        m_renderer.setEditorViewport(m_editorLayer->viewport().presentation(), m_editorLayer->viewport().mode());
        m_renderer.endFrame();
    }

    m_renderingFrame = false;
}

} // namespace Engine
