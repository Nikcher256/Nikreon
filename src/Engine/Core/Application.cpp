#include "Engine/Core/Application.hpp"

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
{
    m_time.reset();
    spdlog::info("Application created.");
    spdlog::info("Vulkan runtime support: {}", glfwVulkanSupported() == GLFW_TRUE ? "available" : "unavailable");
}

Application::~Application()
{
    spdlog::info("Application destroyed.");
}

void Application::run(const RunOptions& options)
{
    spdlog::info("Application loop started.");

    while (!m_window.shouldClose()) {
        m_time.tick();
        m_window.pollEvents();

        update(m_time.deltaSeconds());
        render();

        ++m_frameCount;
        if (options.maxFrames > 0 && m_frameCount >= options.maxFrames) {
            m_window.requestClose();
        }
    }

    spdlog::info("Application loop stopped after {:.2f}s.", m_time.elapsedSeconds());
}

void Application::update(const float deltaTime)
{
    (void)deltaTime;

}

void Application::render()
{
    // Phase 2 will attach the Vulkan frame renderer here.
}

} // namespace Engine
