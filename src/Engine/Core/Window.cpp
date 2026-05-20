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

void Window::toggleFullscreen()
{
    if (m_handle == nullptr) {
        return;
    }

    if (!m_fullscreen) {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        glfwGetWindowPos(m_handle, &x, &y);
        glfwGetWindowSize(m_handle, &width, &height);

        m_windowedX = x;
        m_windowedY = y;
        m_windowedWidth = width > 0 ? static_cast<std::uint32_t>(width) : m_width;
        m_windowedHeight = height > 0 ? static_cast<std::uint32_t>(height) : m_height;

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = monitor != nullptr ? glfwGetVideoMode(monitor) : nullptr;
        if (monitor == nullptr || mode == nullptr) {
            return;
        }

        glfwSetWindowMonitor(m_handle, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        m_fullscreen = true;
        spdlog::info("Window switched to fullscreen ({}x{}).", mode->width, mode->height);
    } else {
        glfwSetWindowMonitor(
            m_handle,
            nullptr,
            m_windowedX,
            m_windowedY,
            static_cast<int>(m_windowedWidth),
            static_cast<int>(m_windowedHeight),
            0);
        m_fullscreen = false;
        spdlog::info("Window restored to windowed mode ({}x{}).", m_windowedWidth, m_windowedHeight);
    }

    m_framebufferResized = true;
}

bool Window::consumeFramebufferResized()
{
    const bool resized = m_framebufferResized;
    m_framebufferResized = false;
    return resized;
}

void Window::setRefreshCallback(RefreshCallback callback)
{
    m_refreshCallback = std::move(callback);
}

bool Window::shouldClose() const
{
    return m_handle == nullptr || glfwWindowShouldClose(m_handle) == GLFW_TRUE;
}

bool Window::isKeyPressed(const int key) const
{
    return m_handle != nullptr && glfwGetKey(m_handle, key) == GLFW_PRESS;
}

bool Window::isFullscreen() const
{
    return m_fullscreen;
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
        self->m_framebufferResized = true;
    });

    glfwSetWindowRefreshCallback(m_handle, [](GLFWwindow* window) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (self == nullptr) {
            return;
        }

        if (self->m_refreshCallback) {
            self->m_refreshCallback();
        }
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
