#include "Engine/Core/Window.hpp"

#include <stdexcept>

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

namespace Engine {

Window::Window(const WindowProps& props)
{
    create(props);
}

Window::~Window()
{
    destroy();
}

void Window::pollEvents() const
{
    glfwPollEvents();
}

void Window::requestClose()
{
    if (m_handle != nullptr) {
        glfwSetWindowShouldClose(m_handle, GLFW_TRUE);
    }
}

bool Window::shouldClose() const
{
    return m_handle == nullptr || glfwWindowShouldClose(m_handle) == GLFW_TRUE;
}

bool Window::isKeyPressed(const int key) const
{
    return m_handle != nullptr && glfwGetKey(m_handle, key) == GLFW_PRESS;
}

std::uint32_t Window::width() const
{
    return m_width;
}

std::uint32_t Window::height() const
{
    return m_height;
}

GLFWwindow* Window::nativeHandle() const
{
    return m_handle;
}

void Window::create(const WindowProps& props)
{
    if (glfwInit() != GLFW_TRUE) {
        throw std::runtime_error("Failed to initialize GLFW.");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_width = props.width;
    m_height = props.height;
    m_handle = glfwCreateWindow(
        static_cast<int>(m_width),
        static_cast<int>(m_height),
        props.title.c_str(),
        nullptr,
        nullptr);

    if (m_handle == nullptr) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window.");
    }

    glfwSetWindowUserPointer(m_handle, this);
    glfwSetFramebufferSizeCallback(m_handle, [](GLFWwindow* window, const int width, const int height) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (self == nullptr) {
            return;
        }

        self->m_width = width > 0 ? static_cast<std::uint32_t>(width) : 0U;
        self->m_height = height > 0 ? static_cast<std::uint32_t>(height) : 0U;
    });

    spdlog::info("Window created: '{}' ({}x{})", props.title, m_width, m_height);
}

void Window::destroy()
{
    if (m_handle != nullptr) {
        glfwDestroyWindow(m_handle);
        m_handle = nullptr;
        spdlog::info("Window destroyed.");
    }

    glfwTerminate();
}

} // namespace Engine
